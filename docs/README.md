# Doxygen Output

This directory is used for generated API documentation.

- Doxygen config: `docs/doxygen.conf`
- Generated HTML output: `docs/api/`
- Entry point after generation: `docs/index.html`
- Warning log: `docs/doxygen-warnings.log`
- Architecture diagram sources:
  - `docs/simulation_flow_diagram.drawio`
  - `docs/simulation_flow_diagram.png`

Generate locally:

```bash
./scripts/generate_doxygen_docs.sh
```

Local requirement:
- `doxygen` command must be installed.

`docs/simulation_flow_diagram.png` is managed as a repository asset.
