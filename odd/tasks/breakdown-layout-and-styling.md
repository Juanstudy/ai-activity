# Breakdown Layout and Styling
Refactor usage breakdown section into an asymmetric two-column layout with enhanced visual hierarchy and tabular metrics.
Solves the "colored JSON" readability issue by separating projects (wide 60% panel) from models and agents (40% right stack), adding path prefix dimming, share progress bars, and structured badge metrics.

## Specs
S1: "vale, ahora trabajemos con la parte inferior dime que podriamos hacer con el para mejorar el order, el estilo y la legibilidad, pues ahora es casi un json con colores"
S2: "1- alternativa B como digas" - Layout Asimétrico 2 Columnas: Proyectos a la izquierda (60%), Modelos y Agentes a la derecha (40%), jerarquía tipográfica y barras de share.

## Tasks
T1 (S1, S2): Overhaul BreakdownList.qml delegate with path hierarchy, structured metric badges, visible share bar, and dimming for $0 items | route: inline | commit: verified
T2 (S1, S2): Reorganize Main.qml breakdown section into 60/40 two-column asymmetric layout with compact agents panel | route: inline | commit: verified
T3 (S1, S2): Verify test suite and offscreen QML load test | route: inline | commit: verified (11/11 tests pass)

## Log
L1: "vale, ahora trabajemos con la parte inferior dime que podriamos hacer con el para mejorar el order, el estilo y la legibilidad, pues ahora es casi un json con colores"
L2: "1- alternativa B como digas"
L3: "All 3 tasks verified. BreakdownList redesigned with path prefix dimming, discrete token/turn chips, relative share % and progress bar. Main.qml reorganized into 60% projects left column and 40% models/agents right stack. 11/11 tests passing."
