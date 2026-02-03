# apps

- Language: English only across the project
- Position: deployable application units (engine/gateway/backtest/ui)
- Responsibilities: compose libs/ and proto/ to provide process-level entrypoints
- Dependencies: may depend on libs/ and proto/; avoid direct dependency on services/ internals
- Change note: new apps should update build and run scripts
