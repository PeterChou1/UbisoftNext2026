# Model import folder

Put `.obj` files (and the `.mtl` files they use) here, then in the
**SceneEditor** type the file name (without `.obj`) in the **import .obj**
box at the bottom of **Assets** and press **Enter**. The model is copied into
`data/models/`, listed under **Models**, and ready to place: click the scene.

You can also type a full path to an `.obj` anywhere on disk.

`pyramid.obj` is an example: type `pyramid` and press Enter.

The importer accepts:

- faces with any number of corners (they are triangulated);
- `v`, `v/vt`, `v//vn` and `v/vt/vn` face corners, including negative
  indices;
- `mtllib` / `usemtl` colours (Ka, Kd, Ks, Ns). A missing `.mtl` falls back
  to the default material.

Models are resized so their widest side is 1 unit and they stand on the
ground. Use the object's **Scale** in the editor to size them.
