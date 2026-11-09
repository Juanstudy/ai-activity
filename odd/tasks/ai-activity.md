# Feature: ai-activity
Desktop analytics app for Omarchy to monitor AI usage, costs in USD/COP, models, thinking tokens, and activity heatmap.

## Specs
- S1: "revisar el uso de ai que llevo en mi laptop, en distintos lapsos de tiempos (5h, 12h, 24h, 7d, 30d, 6m, 1y, y all time)"
- S2: "costo promedio en usd y cop" con "2. sync auto"
- S3: "agentes, proyectos, modelos, thinkigs, etc"
- S4: "visor tipo github de uso"
- S5: "por el momento podemos colocar solo pi, es mi agente predeterminado y de alli creo que hago el 95% de cosas"
- S6: "opcion A" (Nativa Omarchy - C++ / Qt6 Quick / QML con live theme y packaging) en "/projects y colocale ai-activity"

## Tasks
- T1 (S6): Scaffold project structure, qmake6 config, build and test scripts [inline] (commit a5c1009)
- T2 (S1, S3, S5): SQLite database and incremental Pi session parser in storage module [inline] (commit a5c1009)
- T3 (S2): TRM sync client with network access, local cache, and offline fallback [inline] (commit a5c1009)
- T4 (S1, S2, S3, S4): C++ Backend state with range filters, aggregations, and heatmap binning [inline] (commit a5c1009)
- T5 (S1, S4, S6): QML user interface, GitHub-style Heatmap, Omarchy theme integration, and keyboard shortcuts [inline] (commit a5c1009)
- T6 (S1, S2, S3, S4, S5, S6): Offscreen test suite, PKGBUILD, desktop file, icon, and installation verification [inline] (commit a5c1009)

## Log
- L1: "quiero constuir una app que me permita revisar el uso de ai que llevo en mi laptop, en distintos lapsos de tiempos (5h, 12h, 24h, 7d, 30d, 6m, 1y, y all time), costo promedio en usd y cop, agentes, proyectos, modelos, thinkigs, etc, visor tipo github de uso"
- L2: "1- por el momento podemos colocar solo pi, es mi agente predeterminado y de alli creo que hago el 95% de cosas\n2- podes revisar un plugin de omarchy que se llama agent activity que hace algo parecido para entender el core\n3- 2. sync auto\n4- opcion A"
- L3: "hacelo en /projects y colocale ai-activity"
