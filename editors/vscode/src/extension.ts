import * as vscode from "vscode";
import * as cp from "child_process";
import * as os from "os";
import * as path from "path";
import * as fs from "fs";

import { TYPES, CONTROL_KEYWORDS, BUILTINS, CONSTANT_GROUPS } from "./constants";

const LANGUAGE_ID = "mipasm";
const DIAGNOSTIC_SOURCE = "mipasm";

let diagnostics: vscode.DiagnosticCollection;

/** Monotonic run token per document so a stale compiler run never overwrites
 *  the diagnostics of a newer save. */
const runTokens = new Map<string, number>();

export function activate(context: vscode.ExtensionContext): void {
  diagnostics = vscode.languages.createDiagnosticCollection(LANGUAGE_ID);
  context.subscriptions.push(diagnostics);

  context.subscriptions.push(
    vscode.languages.registerCompletionItemProvider(LANGUAGE_ID, {
      provideCompletionItems: () => buildCompletions()
    })
  );

  context.subscriptions.push(
    vscode.workspace.onDidSaveTextDocument((doc) => lint(doc)),
    vscode.workspace.onDidOpenTextDocument((doc) => lint(doc)),
    vscode.workspace.onDidCloseTextDocument((doc) => {
      runTokens.delete(doc.uri.toString());
      diagnostics.delete(doc.uri);
    })
  );

  // Lint documents that are already open when the extension activates.
  vscode.workspace.textDocuments.forEach((doc) => lint(doc));
}

export function deactivate(): void {
  if (diagnostics) {
    diagnostics.dispose();
  }
}

/* ----------------------------- Autocomplete ----------------------------- */

let completionCache: vscode.CompletionItem[] | undefined;

function buildCompletions(): vscode.CompletionItem[] {
  if (completionCache) {
    return completionCache;
  }
  const items: vscode.CompletionItem[] = [];

  for (const keyword of CONTROL_KEYWORDS) {
    items.push(new vscode.CompletionItem(keyword, vscode.CompletionItemKind.Keyword));
  }
  for (const type of TYPES) {
    items.push(new vscode.CompletionItem(type, vscode.CompletionItemKind.Keyword));
  }
  for (const builtin of BUILTINS) {
    const item = new vscode.CompletionItem(builtin.name, vscode.CompletionItemKind.Function);
    item.detail = builtin.detail;
    item.insertText = new vscode.SnippetString(builtin.snippet);
    items.push(item);
  }
  for (const group of CONSTANT_GROUPS) {
    for (const name of group.names) {
      const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Constant);
      item.detail = `${group.detail} (stdlib)`;
      items.push(item);
    }
  }

  completionCache = items;
  return items;
}

/* ------------------------------- Linting -------------------------------- */

function isMipasmDocument(doc: vscode.TextDocument): boolean {
  return (
    doc.uri.scheme === "file" &&
    (doc.languageId === LANGUAGE_ID || doc.fileName.endsWith(".mip"))
  );
}

function lint(doc: vscode.TextDocument): void {
  if (!isMipasmDocument(doc)) {
    return;
  }
  const config = vscode.workspace.getConfiguration("mipasm");
  if (!config.get<boolean>("lintOnSave", true)) {
    diagnostics.delete(doc.uri);
    return;
  }

  const key = doc.uri.toString();
  const token = (runTokens.get(key) ?? 0) + 1;
  runTokens.set(key, token);

  const env = { ...process.env };
  const libPath = (config.get<string>("libPath", "") || "").trim();
  if (libPath) {
    env.MIPASM_LIB_PATH = libPath;
  }

  const candidates = compilerCandidates(config, doc.uri);
  runCandidate(candidates, 0, doc, env, token);
}

/** Try each compiler candidate until one starts; report its diagnostics. */
function runCandidate(
  candidates: string[],
  index: number,
  doc: vscode.TextDocument,
  env: NodeJS.ProcessEnv,
  token: number
): void {
  if (isStale(doc, token)) {
    return;
  }
  if (index >= candidates.length) {
    diagnostics.set(doc.uri, [
      fileLevelDiagnostic(
        "Could not run the mipasm compiler. Set the 'mipasm.compilerPath' setting.",
        vscode.DiagnosticSeverity.Warning
      )
    ]);
    return;
  }

  const compiler = candidates[index];
  const tmpOut = path.join(
    os.tmpdir(),
    `mipasm-lint-${process.pid}-${Date.now()}.mid`
  );

  let stderr = "";
  let startFailed = false;

  const child = cp.spawn(compiler, [doc.uri.fsPath, "-o", tmpOut], { env });
  child.stderr?.on("data", (chunk) => {
    stderr += chunk.toString();
  });
  child.on("error", () => {
    // This candidate is not executable (e.g. not on PATH); try the next one.
    startFailed = true;
    runCandidate(candidates, index + 1, doc, env, token);
  });
  child.on("close", () => {
    if (startFailed) {
      return;
    }
    fs.unlink(tmpOut, () => {
      /* best-effort cleanup */
    });
    if (isStale(doc, token)) {
      return;
    }
    diagnostics.set(doc.uri, parseDiagnostics(stderr, doc));
  });
}

function isStale(doc: vscode.TextDocument, token: number): boolean {
  return runTokens.get(doc.uri.toString()) !== token;
}

/** Ordered list of compiler paths to try. A bare command is tried first (so an
 *  installed `mipasm` on PATH wins); the workspace dev builds are fallbacks. */
function compilerCandidates(
  config: vscode.WorkspaceConfiguration,
  uri: vscode.Uri
): string[] {
  const configured = (config.get<string>("compilerPath", "mipasm") || "mipasm").trim();
  const candidates: string[] = [configured];

  const isBare = !configured.includes("/") && !configured.includes(path.sep);
  if (isBare) {
    const folder = vscode.workspace.getWorkspaceFolder(uri);
    if (folder) {
      for (const rel of [".build/mipasm", ".build-release/mipasm"]) {
        const dev = path.join(folder.uri.fsPath, rel);
        if (fs.existsSync(dev)) {
          candidates.push(dev);
        }
      }
    }
  }
  return candidates;
}

/* ---------------------------- Output parsing ---------------------------- */

// Primary format emitted by the compiler: "line:column: severity: message".
const DIAGNOSTIC_RE = /^(\d+):(\d+):\s*(error|warning|fatal error):\s*(.*)$/i;
// Same, but tolerating a leading "file:" prefix (other build setups).
const DIAGNOSTIC_FILE_RE = /^.+?:(\d+):(\d+):\s*(error|warning|fatal error):\s*(.*)$/i;
// Fallbacks for an un-patched compiler that prints no position.
const LEGACY_FATAL_RE = /^mipasm:\s*(?:fatal\s+)?error:\s*(.*)$/i;
const LEGACY_TAG_RE = /^\[(?:ERROR|WARN)\]\[[^\]]*\]\s*(.*)$/;
// Noise lines we never want to surface as diagnostics.
const SKIP_RE = [
  /phase rejects the input program/i,
  /rejected the program with \d+ error/i,
  /^generation failed\.?$/i
];

function stripAnsi(text: string): string {
  // eslint-disable-next-line no-control-regex
  return text.replace(/\x1b\[[0-9;]*m/g, "");
}

function parseDiagnostics(
  raw: string,
  doc: vscode.TextDocument
): vscode.Diagnostic[] {
  const positioned: vscode.Diagnostic[] = [];
  const fallback: vscode.Diagnostic[] = [];

  for (const rawLine of stripAnsi(raw).split(/\r?\n/)) {
    const line = rawLine.trim();
    if (line.length === 0 || SKIP_RE.some((re) => re.test(line))) {
      continue;
    }

    const match = DIAGNOSTIC_RE.exec(line) ?? DIAGNOSTIC_FILE_RE.exec(line);
    if (match) {
      const lineNo = parseInt(match[1], 10) - 1;
      const colNo = parseInt(match[2], 10) - 1;
      positioned.push(
        positionedDiagnostic(doc, lineNo, colNo, match[4], severityOf(match[3]))
      );
      continue;
    }

    const fatal = LEGACY_FATAL_RE.exec(line);
    if (fatal) {
      fallback.push(fileLevelDiagnostic(fatal[1], vscode.DiagnosticSeverity.Error));
      continue;
    }
    const tagged = LEGACY_TAG_RE.exec(line);
    if (tagged) {
      fallback.push(fileLevelDiagnostic(tagged[1], vscode.DiagnosticSeverity.Error));
    }
  }

  // Prefer precise diagnostics; only show the file-level fallback when the
  // compiler gave us no positioned ones (e.g. an old binary).
  return positioned.length > 0 ? positioned : fallback;
}

function severityOf(word: string): vscode.DiagnosticSeverity {
  return /warning/i.test(word)
    ? vscode.DiagnosticSeverity.Warning
    : vscode.DiagnosticSeverity.Error;
}

function positionedDiagnostic(
  doc: vscode.TextDocument,
  lineNo: number,
  colNo: number,
  message: string,
  severity: vscode.DiagnosticSeverity
): vscode.Diagnostic {
  const safeLine = clamp(lineNo, 0, Math.max(0, doc.lineCount - 1));
  const textLine = doc.lineAt(safeLine);
  const safeCol = clamp(colNo, 0, Math.max(0, textLine.text.length));

  // Squiggle the token under the reported column when there is one, otherwise
  // the rest of the line.
  const wordRange = doc.getWordRangeAtPosition(new vscode.Position(safeLine, safeCol));
  const range =
    wordRange ??
    new vscode.Range(safeLine, safeCol, safeLine, Math.max(safeCol + 1, textLine.text.length));

  const diagnostic = new vscode.Diagnostic(range, message, severity);
  diagnostic.source = DIAGNOSTIC_SOURCE;
  return diagnostic;
}

function fileLevelDiagnostic(
  message: string,
  severity: vscode.DiagnosticSeverity
): vscode.Diagnostic {
  const diagnostic = new vscode.Diagnostic(
    new vscode.Range(0, 0, 0, 1),
    message,
    severity
  );
  diagnostic.source = DIAGNOSTIC_SOURCE;
  return diagnostic;
}

function clamp(value: number, min: number, max: number): number {
  return Math.min(Math.max(value, min), max);
}
