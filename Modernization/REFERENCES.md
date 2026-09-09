# External Technical References

These references are informational inputs for implementation and format validation. The repository's own tests and documented compatibility targets remain authoritative for this project.

## W3D / W3X

### OpenSAGE file-format documentation

- W3D documentation: `https://opensage.readthedocs.io/file-formats/w3d/`
- Notes that later SAGE games use W3X as an XML-based evolution of W3D.

### OpenSAGE Blender Plugin

- Repository: `https://github.com/OpenSAGE/OpenSAGE.BlenderPlugin`
- Community W3D/W3X importer/exporter useful as an interoperability reference and for understanding practical content workflows.

### EA C&C Modding Support

- Repository: `https://github.com/electronicarts/CnC_Modding_Support`
- Public later-SAGE XML/modding examples can help identify W3X references, naming conventions, and companion asset declarations.

## Direct3D 12

Prefer current Microsoft documentation for API contracts and recommended patterns:

- Direct3D 12 programming guide: `https://learn.microsoft.com/windows/win32/direct3d12/directx-12-programming-guide`
- DXGI documentation: `https://learn.microsoft.com/windows/win32/direct3ddxgi/dx-graphics-dxgi`
- DXC project/documentation: `https://github.com/microsoft/DirectXShaderCompiler`

## Reference-use rules

- Do not copy external code without checking license compatibility and attribution obligations.
- Prefer behavioral tests and minimal clean implementations over importing large foreign subsystems.
- Do not use proprietary game assets as repository fixtures unless redistribution is legally permitted.
- Synthetic/minimal test fixtures are preferred for W3X parser tests.
