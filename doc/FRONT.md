directives -> include_stmt directives | ε
include_stmt -> #include STRING_LIT
global_declarations -> declaration global_declarations | ε
Funciones y BloquesPlaintextmain_func -> void main ( ) block
block -> { statements }
statements -> statement statements | ε
Sentencias (Statements)Plaintextstatement -> declaration 
 # FRONT — Especificación del Frontend

 Este documento resume las fases del Frontend: análisis léxico, análisis sintáctico y generación del AST.

 ## 1. Análisis Léxico (Alfabeto y Tokens)

 El analizador léxico (implementable con herramientas como Flex) agrupa el flujo de caracteres en tokens ($\Sigma$).

 ### 1.1 Palabras reservadas (keywords)

 Las siguientes cadenas tienen significado reservado y no pueden usarse como identificadores:

 - Tipos de datos: `void`, `int`, `float`, `boolean`, `track`, `duration`
 - Estructura y control: `main`, `if`, `else`, `for`, `while`, `sync`, `const`
 - Directivas: `#include`
 - Funciones nativas de dominio: `init_track`, `play`, `rest`, `set_volume`, `set_pan`, `set_attack`

 ### 1.2 Operadores y símbolos (delimitadores)

 - Aritméticos: `+`, `-`, `*`, `/`
 - Relacionales: `==`, `!=`, `<`, `>`, `<=`, `>=`
 - Lógicos: `&&`, `||`, `!`
 - Asignación: `=`
 - Agrupación y delimitadores: `{`, `}`, `(`, `)`, `,`, `;`

 ### 1.3 Expresiones regulares (patrones)

 Tokens dinámicos y sus patrones:

 | Token | Expresión regular | Descripción |
 | ----- | ----------------- | ----------- |
 | `ID` | `[a-zA-Z_][a-zA-Z0-9_]*` | Identificadores (variables, pistas). |
 | `INT_LIT` | `[0-9]+` | Literales enteros (ej. notas MIDI). |
 | `FLOAT_LIT` | `[0-9]*\.[0-9]+` | Literales flotantes (duraciones relativas). |
 | `STRING_LIT` | `"<[a-zA-Z0-9_./]+>"` | Rutas en `#include`. |
 | `LINE_COMMENT` | `//.*` | Comentarios de una sola línea (ignorados). |
 | `WHITESPACE` | `[ \t\n\r]+` | Espacios y saltos de línea (ignorados). |

 ## 2. Análisis Sintáctico (Gramática formal)

 El parser (Bison) verifica que la secuencia de tokens cumpla la Gramática Libre de Contexto $G = \langle\Sigma, N, \Pi, S\rangle$.

 ### 2.1 Precedencia y asociatividad

 Para resolver ambigüedades y conflictos shift/reduce, se define la precedencia (de menor a mayor):

 1. `=` (asignación) — asociatividad derecha
 2. `||` (OR lógico) — asociatividad izquierda
 3. `&&` (AND lógico) — asociatividad izquierda
 4. `==`, `!=` (igualdad) — asociatividad izquierda
 5. `<`, `<=`, `>`, `>=` (relacionales) — asociatividad izquierda
 6. `+`, `-` (suma/resta) — asociatividad izquierda
 7. `*`, `/` (multiplicación/división) — asociatividad izquierda
 8. `!` (NOT lógico) — asociatividad derecha

 Nota: el conflicto de "dangling else" se resuelve asociando el `else` al `if` más cercano (comportamiento por defecto en Bison).

 ### 2.2 Producciones (\Pi)

 Símbolo inicial: `program`

 ```bnf
 program                -> directives global_declarations main_func

 directives             -> include_stmt directives
                        | /* empty */

 include_stmt           -> #include STRING_LIT

 global_declarations    -> declaration global_declarations
                        | /* empty */

 main_func              -> void main ( ) block

 block                  -> { statements }

 statements             -> statement statements
                        | /* empty */

 statement              -> declaration
                        | assignment ;
                        | track_init
                        | play_stmt
                        | rest_stmt
                        | sync_block
                        | control_stmt
                        | cc_stmt

 declaration            -> type ID ;
                        | type ID = expression ;
                        | const type ID = expression ;

 type                   -> int | float | boolean | track | duration

 assignment             -> ID = expression

 track_init             -> track ID = init_track ( expression ) ;

 play_stmt              -> play ( ID , expression , expression ) ;

 rest_stmt              -> rest ( ID , expression ) ;

 sync_block             -> sync block

 cc_stmt                -> set_volume ( ID , expression ) ;
                        | set_pan ( ID , expression ) ;
                        | set_attack ( ID , expression ) ;

 control_stmt           -> if_stmt | for_stmt | while_stmt

 if_stmt                -> if ( expression ) block
                        | if ( expression ) block else block

 for_stmt               -> for ( type ID = expression ; expression ; assignment ) block
                        | for ( ID = expression ; expression ; assignment ) block

 while_stmt             -> while ( expression ) block

 expression             -> expression + expression
                        | expression - expression
                        | expression * expression
                        | expression / expression
                        | expression < expression
                        | expression > expression
                        | expression <= expression
                        | expression >= expression
                        | expression == expression
                        | expression != expression
                        | expression && expression
                        | expression || expression
                        | ! expression
                        | ( expression )
                        | ID
                        | INT_LIT
                        | FLOAT_LIT
 ```

 ## 3. Árbol de Sintaxis Abstracta (AST)

 El AST es la representación estructurada que produce el frontend cuando el parser reduce producciones. Se eliminan tokens sintácticos innecesarios (paréntesis, `;`, `{`, `}`) y se preserva la estructura semántica.

 Ejemplo — producción `play_stmt`:

 ```text
 play_stmt -> play ( ID , expression , expression ) ;

 Nodo: AST_PLAY
 - targetTrack : Nodo `ID`
 - noteValue   : Sub-árbol de `expression` (tono MIDI)
 - duration    : Sub-árbol de `expression` (duración relativa)
 ```