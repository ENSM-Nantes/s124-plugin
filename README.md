# S-124 Navigational Warnings (OpenCPN plugin)

OpenCPN plugin for displaying IHO S-124 navigational warnings on the chart.

## Features

- Load S-124 GML files from disk (single file or entire folder)
- Fetch live warnings from one or more SECOM service endpoints, with automatic
  pagination and optional periodic auto-refresh
- Renders warning areas, lines, and points as overlays on the chart, colored
  by warning severity (Local, Coastal, Sub-area, NAVAREA) — colors are
  user-configurable and apply the same way across every SECOM connection
- Click any warning marker or area on the chart to read its full text

## SECOM Service URLs (France — PING / SHOM)

Enter one of these URLs in the **SECOM Base URL** field of the plugin settings.

> No API key is required for public access. Leave the API Key field empty.

### AVURNAV (navigational warnings)

| Series | Base URL |
|---|---|
| NAVAREA II | `https://services.ping-info-nautique.fr/secom/navarea_ii_fr` |
| AVURNAV BREST | `https://services.ping-info-nautique.fr/secom/avurnav_brest_fr` |
| AVURNAV CHERBOURG | `https://services.ping-info-nautique.fr/secom/avurnav_cherbourg_fr` |
| AVURNAV TOULON | `https://services.ping-info-nautique.fr/secom/avurnav_toulon_fr` |
| AVURNAV CAYENNE | `https://services.ping-info-nautique.fr/secom/avurnav_cayenne_fr` |
| AVURNAV FORT DE FRANCE | `https://services.ping-info-nautique.fr/secom/avurnav_fort_de_france_fr` |
| AVURNAV LA REUNION | `https://services.ping-info-nautique.fr/secom/avurnav_la_reunion_fr` |
| AVURNAV LOCAL BREST | `https://services.ping-info-nautique.fr/secom/avurnav_local_brest_fr` |
| AVURNAV LOCAL CHERBOURG | `https://services.ping-info-nautique.fr/secom/avurnav_local_cherbourg_fr` |
| AVURNAV LOCAL TOULON | `https://services.ping-info-nautique.fr/secom/avurnav_local_toulon_fr` |
| AVURNAV LOCAL CAYENNE | `https://services.ping-info-nautique.fr/secom/avurnav_local_cayenne_fr` |
| AVURNAV LOCAL FORT DE FRANCE | `https://services.ping-info-nautique.fr/secom/avurnav_local_fort_de_france_fr` |
| AVURNAV LOCAL LA REUNION | `https://services.ping-info-nautique.fr/secom/avurnav_local_la_reunion_fr` |
| AVURNAV LOCAL PAPEETE | `https://services.ping-info-nautique.fr/secom/avurnav_local_papeete_fr` |

### AVINAV (inland waterway notices)

| Series | Base URL |
|---|---|
| AVINAV BREST | `https://services.ping-info-nautique.fr/secom/avinav_brest_fr` |
| AVINAV CHERBOURG | `https://services.ping-info-nautique.fr/secom/avinav_cherbourg_fr` |
| AVINAV TOULON | `https://services.ping-info-nautique.fr/secom/avinav_toulon_fr` |
| AVINAV CAYENNE | `https://services.ping-info-nautique.fr/secom/avinav_cayenne_fr` |
| AVINAV FORT DE FRANCE | `https://services.ping-info-nautique.fr/secom/avinav_fort_de_france_fr` |
| AVINAV LA REUNION | `https://services.ping-info-nautique.fr/secom/avinav_la_reunion_fr` |
| AVINAV PAPEETE | `https://services.ping-info-nautique.fr/secom/avinav_papeete_fr` |

### AVIRADE (port and harbour notices)

| Series | Base URL |
|---|---|
| AVIRADE BREST | `https://services.ping-info-nautique.fr/secom/avirade_brest_fr` |
| AVIRADE CHERBOURG | `https://services.ping-info-nautique.fr/secom/avirade_cherbourg_fr` |

## Version history

- **1.0** — Renamed to S-124 Navigational Warnings; version numbering reset
- **4.0** — SECOM ZIP (S-100 Exchange Set) decoding; 1-based pagination; pageSize capped at 40
- **3.2** — SECOM searchService POST with pagination; open folder menu option
- **3.1** — Open S-124 folder menu option
