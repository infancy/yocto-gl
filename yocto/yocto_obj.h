//
// YOCTO_OBJ: Wavefront OBJ/MTL loader and writer with support for points,
// lines, triangles and general polygons and all materials properties.
// Contains also a few extension to eqasily create demos such as per-vertex
// color and radius, cameras and envmaps.
//

//
// USAGE FOR READING:
//
// 0. include this file (more compilation options below)
// 1. load an obj with yo_load_obj
//   - loads an obj from disk including its associate mtl files
//   - returns a parsed scene data structure described below
//   - optionally support triangulation on loads that makes the loader
//     faster (use it alwayd if you do not need quads/polys/polylines)
//   - extension can be optionally enabled
//   scene = yo_load_obj(filename, triangulate, enable_extensions)
//   1.a. optionally load textures data as float arrays with
//   yo_load_textures(scene, scene_filename, req_comp)
// 2. access the data directly from the returned scene object
//   - has five main arrays: shapes, materials, textures, cameras, envmaps
//   e.g. for(int i = 0; i < scene->nshapes; i ++) scene->shapes[i].XXX
// 3. cleanup with yo_free_scene
//   - you have to do this for each shape bvh and the scene bvh
//   yb_free_bvh(bvh)
//
// The interface for each function is described in details in the interface
// section of this file.
//
// Shapes are indexed meshes and are described by their
// number of elements, an array of vertex indices,
// the primitive type (points, lines, triangles, quads),
// and arrays for vertex positions, normals, texcoords, color and radius.
// The latter two as extensions.
//
// Faces in the scene have the same number of elements for points (1),
// lines (2), triangles (3). We also
// support general polygons and polines with arbitrary number of faces.
// To avdoi wasting memory, these are saved sequentially where the first int
// in the element is the number od vertices. While this does not allow
// random access, it saves significant memory and avoid pointer chasing.
//
// Since OBJ is a complex formats that does not match well with current
// GPU rendering / path tracing algorithms, we adopt a simplification similar
// to other single file libraries.
//
// 1. vertex indices are unique, as in OpenGL and al standard indexed triangle
//   meshes data structures, and not OBJ triplets; YOCTO_OBJ ensures that no
//   vertex dusplication happens thought for same triplets
// 2. we split shapes on changes to groups and materials, instead of keeping
//   per-face group/material data; this makes the data usable right away in
//   a GPU viewer; this is not a major limitation if we accept the previous
//   point that already changes shapes topology.
//

//
// USAGE FOR WRITING:
//
// 0. include this file (more compilation options below)
// 1. fill a yo_scene data with your scene data
//    - note that if your shape data is layed out in memore as ours,
//    - then no copy is needed, just set the pointers
// 2. save the obj/mtl pair with yo_load_obj
//    yo_save_obj(filename, scene, enable_extensions)
// 3. If you copies memory over, clear it with yo_free_scene
//

//
// USAGE FOR BINARY DUMPS:
//
// 1. you can also have binary dumps used for fast data access with
//    scene = yo_load_objbin(filename, enable_extensions) and
//    yo_save_objbin(filename, scene, ext)
// 2. These files are just binary dumps, so should not be used for
//    archival but as a speed up to avoid ASCII serializatiion/deserialization
//

//
// COMPILATION:
//
// All functions in this library are inlined by default for ease of use in C++.
// To use the library as a .h/.cpp pair do the following:
// - to use as a .h, just #define YGL_DECLARATION before including this file
// - to build as a .cpp, just #define YGL_IMPLEMENTATION before including this
// file into only one file that you can either link directly or pack as a lib.
//
// This file depends on yocto_math.h.
//

//
// HISTORY:
// - v 0.2: removal of C interface
// - v 0.1: C++ implementation
// - v 0.0: initial release in C99
//

//
// LICENSE:
//
// Copyright (c) 2016 Fabio Pellacini
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#ifndef _YO_H_
#define _YO_H_

// compilation options
#ifndef YGL_DECLARATION
#define YGL_API inline
#else
#define YGL_API
#endif

#include <string>
#include <vector>

#include "yocto_math.h"

// -----------------------------------------------------------------------------
// INTERFACE
// -----------------------------------------------------------------------------

//
// Types of geometric primitives
//
enum {
    yo_etype_null = 0,       // invalid prim to indicate parsing erros
    yo_etype_point = 1,      // points
    yo_etype_line = 2,       // lines
    yo_etype_triangle = 3,   // triangles
    yo_etype_polyline = 12,  // polylines
    yo_etype_polygon = 13    // polygons
};

//
// Geometric shape
//
struct yo_shape {
    // whole shape data
    ym_string name;       // shape name
    ym_string groupname;  // groupname (unique group for each shape object)
    ym_string matname;    // material name
    int matid = -1;       // index in the material array (-1 if not found)

    // shape elements
    int nelems = 0;  // number of elements (point, lines, triangles, etc.)
    std::vector<int> elem;  // per-element vertex indices
    int etype = 0;          // element type from the above enum

    // vertex data
    int nverts = 0;                  // number of vertices
    std::vector<ym_vec3f> pos;       // per-vertex position (3 float)
    std::vector<ym_vec3f> norm;      // per-vertex normals (3 float)
    std::vector<ym_vec2f> texcoord;  // per-vertex texcoord (2 float)
    std::vector<ym_vec3f> color;     // [extension] per-vertex color (3 float)
    std::vector<float> radius;       // [extension] per-vertex radius (1 float)

    // transform
    bool xformed = false;  // [extension] whether a transform is present
    ym_affine3f xform = ym_identity_affine3f;  // [extension] 3x4 affine
                                               // transform matrix (column
                                               // major)
};

//
// Material
//
struct yo_material {
    // whole material data
    ym_string name;  // material name
    int illum = 0;   // MTL illum mode

    // color information
    ym_vec3f ke = ym_zero3f;  // emission color
    ym_vec3f ka = ym_zero3f;  // ambient color
    ym_vec3f kd = ym_zero3f;  // diffuse color
    ym_vec3f ks = ym_zero3f;  // specular color
    ym_vec3f kr = ym_zero3f;  // reflection color
    ym_vec3f kt = ym_zero3f;  // transmision color
    float ns = 1;             // phong exponent for ks
    float ior = 1;            // index of refraction
    float op = 1;             // opacity

    // texture names for the above properties
    ym_string ke_txt;
    ym_string ka_txt;
    ym_string kd_txt;
    ym_string ks_txt;
    ym_string kr_txt;
    ym_string kt_txt;
    ym_string ns_txt;
    ym_string op_txt;
    ym_string ior_txt;
    ym_string bump_txt;  // bump map texture (heighfield)
    ym_string disp_txt;  // displacement map texture (heighfield)

    // indices in the texture array (-1 if not found)
    int ke_txtid = -1;
    int ka_txtid = -1;
    int kd_txtid = -1;
    int ks_txtid = -1;
    int kr_txtid = -1;
    int kt_txtid = -1;
    int ns_txtid = -1;
    int op_txtid = -1;
    int ior_txtid = -1;
    int bump_txtid = -1;
    int disp_txtid = -1;
};

//
// [Extension] Texture
//
struct yo_texture {
    ym_string path;             // path
    int width = 0, height = 0;  // if loaded, image width and hieght
    int ncomp = 0;              // if loaded, number of component (1-4)
    std::vector<float> pixels;  // if loaded, pixel data
};

//
// [Extension] Camera represented as a lookat.
//
struct yo_camera {
    ym_string name;               // name
    ym_vec3f from = ym_zero3f;    // camera position
    ym_vec3f to = ym_z3f;         // camera focus location
    ym_vec3f up = ym_y3f;         // camera up vector
    float width = 1, height = 1;  // image plane width and height
    float aperture = 0;           // lens aperture
};

//
// [Extension] Envinonment map in latlong format
//
struct yo_env {
    ym_string name;     // name
    ym_string matname;  // material name (where only ke, ke_txt are valid)
    int matid = -1;     // index of material in material array (-1 if not found)
    ym_vec3f from = ym_zero3f, to = ym_z3f,
             up = ym_y3f;  // lookat transform data as in yo_camera
};

//
// Scene
//
struct yo_scene {
    std::vector<yo_shape> shapes;        // shape array
    std::vector<yo_material> materials;  // material array
    std::vector<yo_texture> textures;    // texture array
    std::vector<yo_camera> cameras;      // camera array
    std::vector<yo_env> envs;            // environment array
};

//
// Loads a scene from disk
//
// Parameters:
// - filename: scene filename
// - truangulate: whether to triagulate on load (fan-style)
// - ext: enable extensions
//
// Returns:
// - loaded scene or NULL for error
//
YGL_API yo_scene* yo_load_obj(const std::string& filename, bool triangulate,
                              bool ext);

//
// Loads a binary scene dump from disk
//
// Parameters:
// - filename: scene filename
// - ext: enable extensions
//
// Returns:
// - loaded scene or NULL for error
//
YGL_API yo_scene* yo_load_objbin(const std::string& filename, bool ext);

//
// Saves a scene to disk
//
// Parameters:
// - filename: scene filename
// - scene: scene to save
// - ext: enable extensions
//
// Returns:
// - true if ok
//
YGL_API bool yo_save_obj(const std::string& filename, const yo_scene* scene,
                         bool ext);

//
// Saves a binary scene dump to disk
//
// Parameters:
// - filename: scene filename
// - scene: scene to save
// - ext: enable extensions
//
// Returns:
// - true if ok
//
YGL_API bool yo_save_objbin(const std::string& filename, const yo_scene* scene,
                            bool ext);

//
// Free scene data.
//
YGL_API void yo_free_scene(yo_scene* scene);

//
// Loads textures.
//
// Parameters:
// - scene: scene to load into
// - filename: scene filename, used to resolve path references
// - req_comp: 0 for default or 1-4 to force all textures to have the given
//    number of components
//
#ifndef YO_NOIMG
YGL_API void yo_load_textures(yo_scene* scene, const std::string& filename,
                              int req_comp);
#endif

// -----------------------------------------------------------------------------
// DEPRECATED INTERFACE
// -----------------------------------------------------------------------------

//
// Wrapper. See above.
//
YGL_API yo_scene* yo_load_obj(const char* filename, bool triangulate, bool ext);

//
// Wrapper. See above.
//
YGL_API yo_scene* yo_load_objbin(const char* filename, bool ext);

//
// Wrapper. See above.
//
YGL_API bool yo_save_obj(const char* filename, const yo_scene* scene, bool ext);

//
// Wrapper. See above.
//
YGL_API bool yo_save_objbin(const char* filename, const yo_scene* scene,
                            bool ext);

//
// Wrapper. See above.
//
#ifndef YO_NOIMG
YGL_API void yo_load_textures(yo_scene* scene, const char* filename,
                              int req_comp);
#endif

// -----------------------------------------------------------------------------
// IMPLEMENTATION
// -----------------------------------------------------------------------------

#if !defined(YGL_DECLARATION) || defined(YGL_IMPLEMENTATION)

#include <cctype>
#include <cstdio>

//
// OBJ vertex reference triplet (pos,texcoord,norm) with extension for
// color and radius indices. Contains also the vertex unique index in the
// flattened array.
//
struct yo__vert {
    int pos, texcoord, norm, color, radius, vid;
};

//
// OBJ vertex data
//
struct yo__vertdata {
    std::vector<ym_vec3f> pos;
    std::vector<ym_vec2f> texcoord;
    std::vector<ym_vec3f> norm;
    std::vector<ym_vec3f> color;
    std::vector<float> radius;
};

//
// OBJ element data
//
struct yo__elemdata {
    int etype;
    std::vector<int> elem;
};

//
// Vertex hash table to avoid duplicating vertices.
//
#define yo__vhash_size 1048576
struct yo__vhash {
    int nverts;                               // numner of vertices
    std::vector<yo__vert> v[yo__vhash_size];  // bucket data (one vertex for
    // each unique face index)
};

//
// String comparison without case.
// modified from http://clc-wiki.net/wiki/C_standard_library%3astring.h%3astrcmp
//
static inline int yo__stricmp(const char* s1, const char* s2) {
    while (*s1 && (tolower(*s1) == tolower(*s2))) s1++, s2++;
    return tolower(*(const unsigned char*)s1) -
           tolower(*(const unsigned char*)s2);
}

// -----------------------------------------------------------------------------
// OBJ LOADING
// -----------------------------------------------------------------------------

//
// Free scene memory.
//
YGL_API void yo_free_scene(yo_scene* scene) { delete scene; }

//
// During parsing, flashes a shape into the scene if elements are present.
//
YGL_API void yo__add_shape(std::vector<yo_shape>& shapes,
                           const std::vector<yo_material>& materials,
                           const ym_string& name, const ym_string& matname,
                           const ym_string& groupname, const ym_affine3f& xform,
                           yo__elemdata& elem, yo__vertdata& vert,
                           yo__vhash* vhash) {
    // exit if nothing to do
    if (!elem.elem.size()) return;

    // add shape
    shapes.push_back(yo_shape());
    yo_shape* shape = &shapes[shapes.size() - 1];

    // set name
    shape->name = name;
    shape->matname = matname;
    shape->groupname = groupname;

    // set material id
    shape->matid = -1;
    for (int i = 0; i < materials.size() && shape->matid < 0; i++) {
        if (!yo__stricmp(shape->matname.c_str(), materials[i].name.c_str()))
            shape->matid = i;
    }

    // set xform
    shape->xformed = !(xform == ym_identity_affine3f);
    shape->xform = xform;

    // set nverts and check vertex lengths
    shape->nverts = (int)vert.pos.size();
    assert(shape->nverts == vert.pos.size() || !vert.pos.size());
    assert(shape->nverts == vert.norm.size() || !vert.norm.size());
    assert(shape->nverts == vert.texcoord.size() || !vert.texcoord.size());
    assert(shape->nverts == vert.color.size() || !vert.color.size());
    assert(shape->nverts == vert.radius.size() || !vert.radius.size());

    // copy vertices
    shape->pos = vert.pos;
    shape->norm = vert.norm;
    shape->texcoord = vert.texcoord;
    shape->color = vert.color;
    shape->radius = vert.radius;

    // handle simple cases for elements
    if (elem.etype == yo_etype_point || elem.etype == yo_etype_line ||
        elem.etype == yo_etype_triangle) {
        shape->etype = elem.etype;
        shape->nelems = (int)elem.elem.size() / elem.etype;
        shape->elem = elem.elem;
    } else if (elem.etype == yo_etype_polygon ||
               elem.etype == yo_etype_polyline) {
        // tries to compress generic polygon and polylines
        // find size
        int nelems = (int)elem.elem.size();
        int* elemd = elem.elem.data();
        shape->nelems = 0;
        int maxf = -1, minf = 1000000;
        for (int f = 0; f < nelems;) {
            int nf = elemd[f];
            if (nf > maxf) maxf = nf;
            if (nf < minf) minf = nf;
            f += nf + 1;
            shape->nelems++;
        }
        assert(minf > 0);

        // make lines and triangles
        if (minf == maxf && maxf < 4) {
            shape->etype = maxf;
            shape->elem.resize(shape->nelems * shape->etype);
            for (int e = 0; e < shape->nelems; e++)
                memcpy(shape->elem.data() + e * maxf,
                       elemd + e * (maxf + 1) + 1, sizeof(int) * maxf);
        } else {
            shape->etype = elem.etype;
            shape->elem = elem.elem;
        }
    } else {
        assert(false);
    }

    // clear buffers
    vhash->nverts = 0;
    for (int i = 0; i < yo__vhash_size; i++) vhash->v[i].resize(0);
    vert.pos.resize(0);
    vert.norm.resize(0);
    vert.texcoord.resize(0);
    vert.color.resize(0);
    vert.radius.resize(0);
    elem.elem.resize(0);
    elem.etype = 0;
}

//
// Splits a string into an array of strings on whitespace with Python split
// semantic. Modifies original string to avoid allocation.
//
static inline int yo__splitws(char* str, char** splits, int maxsplits) {
    int n = 0;
    while (*str && n < maxsplits) {
        if (isspace(*str)) {
            *str = 0;
        } else {
            if (n == 0 || !(*(str - 1))) {
                splits[n] = str;
                n++;
            }
        }
        str++;
    }
    return n;
}

//
// Add an empty material.
//
static inline void yo__add_empty_material(std::vector<yo_material>& materials,
                                          const ym_string& name) {
    materials.push_back(yo_material());
    yo_material* mat = &materials[materials.size() - 1];
    mat->name = name;
}

//
// Add a camera from OBJ vertices.
//
static inline void yo__add_camera(std::vector<yo_camera>& cameras,
                                  const ym_string& name, const yo__vert& from,
                                  const yo__vert& to,
                                  const yo__vertdata& obj_vert,
                                  yo__vhash* vhash) {
    cameras.push_back(yo_camera());
    yo_camera* cam = &cameras[cameras.size() - 1];

    cam->name = name;
    cam->from = obj_vert.pos[from.pos];
    cam->to = obj_vert.pos[to.pos];
    cam->up = (from.norm >= 0) ? obj_vert.norm[from.norm] : ym_vec3f{0, 1, 0};
    cam->width = (to.texcoord >= 0) ? obj_vert.texcoord[to.texcoord].x : 1;
    cam->height = (to.texcoord >= 0) ? obj_vert.texcoord[to.texcoord].y : 1;
    cam->aperture =
        (from.texcoord >= 0) ? obj_vert.texcoord[from.texcoord].x : 0;

    // clear buffers
    vhash->nverts = 0;
    for (int i = 0; i < yo__vhash_size; i++) vhash->v[i].resize(0);
}

//
// Add an environment map from OBJ vertices.
//
static inline void yo__add_env(std::vector<yo_env>& envs, const ym_string& name,
                               const ym_string& matname, const yo__vert& from,
                               const yo__vert& to, const yo__vertdata& obj_vert,
                               yo__vhash* vhash) {
    envs.push_back(yo_env());
    yo_env* env = &envs[envs.size() - 1];

    env->name = name;
    env->matname = matname;
    env->from = obj_vert.pos[from.pos];
    env->to = obj_vert.pos[to.pos];
    env->up = (from.norm >= 0) ? obj_vert.norm[from.norm] : ym_vec3f{0, 1, 0};

    // clear buffers
    vhash->nverts = 0;
    for (int i = 0; i < yo__vhash_size; i++) vhash->v[i].resize(0);
}

// parses one float
static inline float yo__parse_float(char** tok) { return atof(tok[0]); }

// parses two floats
static inline ym_vec2f yo__parse_float2(char** tok) {
    return ym_vec2f{(float)atof(tok[0]), (float)atof(tok[1])};
}

// parses three floats
static inline ym_vec3f yo__parse_float3(char** tok) {
    return ym_vec3f{(float)atof(tok[0]), (float)atof(tok[1]),
                    (float)atof(tok[2])};
}

// parses 12 floats
static inline ym_affine3f yo__parse_affine3f(char** tok) {
    ym_affine3f m;
    float* mm = (float*)&m;
    for (int i = 0; i < 12; i++) mm[i] = (float)atof(tok[i]);
    return m;
}

// parses an OBJ vertex triplet (or quintuplet with extensions); handle
// nagative indices directly
static inline yo__vert yo__parse_vert(char* str, yo__vhash* vhash,
                                      const yo__vertdata& obj_vert) {
    // parse triplet
    char* splits[] = {str, 0, 0, 0, 0};
    int ns = 1;
    yo__vert v = {-1, -1, -1, -1, -1};
    while (*str) {
        if (*str == '/') {
            *str = 0;
            if (ns < 5) splits[ns++] = str + 1;
        }
        str++;
    }
    int* f = &v.pos;
    yo__vert vl = {(int)obj_vert.pos.size(),    (int)obj_vert.texcoord.size(),
                   (int)obj_vert.norm.size(),   (int)obj_vert.color.size(),
                   (int)obj_vert.radius.size(), 0};
    int* l = &vl.pos;
    for (int i = 0; i < 5; i++) {
        if (!splits[i]) {
            f[i] = -1;
            continue;
        }
        f[i] = (int)atoi(splits[i]);
        f[i] = (f[i] < 0) ? l[i] + f[i] : f[i] - 1;
    }

    // determine position vid using vertex hash
    int pos = -1;
    int hidx = v.pos % yo__vhash_size;
    for (int i = 0; i < vhash->v[hidx].size() && pos < 0; i++) {
        if (v.pos == vhash->v[hidx][i].pos &&
            v.texcoord == vhash->v[hidx][i].texcoord &&
            v.norm == vhash->v[hidx][i].norm &&
            v.color == vhash->v[hidx][i].color &&
            v.radius == vhash->v[hidx][i].radius)
            pos = i;
    }

    // found, can exit
    if (pos >= 0) return vhash->v[hidx][pos];

    // insert in vhash
    v.vid = vhash->nverts;
    vhash->v[hidx].push_back(v);
    vhash->nverts++;

    return v;
}

// add a unique vertex to a parsed shape
static inline void yo__add_shape_vert(yo__vertdata* vert, const yo__vert& v,
                                      const yo__vertdata& obj_vert) {
    // check already added
    if (v.vid < vert->pos.size()) return;
    // TODO: assert for malformed stuff
    if (v.pos >= 0) vert->pos.push_back(obj_vert.pos[v.pos]);
    if (v.norm >= 0) vert->norm.push_back(obj_vert.norm[v.norm]);
    if (v.texcoord >= 0)
        vert->texcoord.push_back(obj_vert.texcoord[v.texcoord]);
    if (v.color >= 0) vert->color.push_back(obj_vert.color[v.color]);
    if (v.radius >= 0) vert->radius.push_back(obj_vert.radius[v.radius]);
}

//
// add a unique texture
//
static inline int yo__add_unique_texture(std::vector<yo_texture>& textures,
                                         const ym_string& path) {
    if (path.empty()) return -1;
    int pos = -1;
    for (int i = 0; i < textures.size() && pos < 0; i++)
        if (textures[i].path == path) pos = i;
    if (pos >= 0) return pos;
    textures.push_back(yo_texture());
    textures[textures.size() - 1].path = path;
    return (int)textures.size() - 1;
}

//
// loads an MTL file
//
static inline bool yo__load_mtl(std::vector<yo_material>& materials,
                                std::vector<yo_texture>& textures,
                                const std::string& filename) {
    FILE* mfile = fopen(filename.c_str(), "rt");
    if (!mfile) return false;

    char mline[4096];
    char* mtok[10];
    int mlinenum = 0;

    yo_material* mat = 0;

    // foreach line, splits the line by whitespaces and parses the data
    // directly in the material
    while (fgets(mline, 4096, mfile)) {
        mlinenum += 1;
        int mntok = yo__splitws(mline, mtok, 10);

        if (!mntok) {
            continue;
        } else if (mtok[0][0] == '#' || mtok[0][0] == '/') {
            continue;
        } else if (!strcmp(mtok[0], "newmtl")) {
            yo__add_empty_material(materials, ym_string(mtok[1]));
            mat = &materials[materials.size() - 1];
        } else if (!strcmp(mtok[0], "illum")) {
            mat->illum = atoi(mtok[1]);
        } else if (!strcmp(mtok[0], "Ke")) {
            mat->ke = yo__parse_float3(mtok + 1);
        } else if (!strcmp(mtok[0], "Ka")) {
            mat->ka = yo__parse_float3(mtok + 1);
        } else if (!strcmp(mtok[0], "Kd")) {
            mat->kd = yo__parse_float3(mtok + 1);
        } else if (!strcmp(mtok[0], "Ks")) {
            mat->ks = yo__parse_float3(mtok + 1);
        } else if (!strcmp(mtok[0], "Kr")) {
            mat->kr = yo__parse_float3(mtok + 1);
        } else if (!strcmp(mtok[0], "Tr")) {
            mat->kt = yo__parse_float3(mtok + 1);
        } else if (!strcmp(mtok[0], "Ns")) {
            mat->ns = yo__parse_float(mtok + 1);
        } else if (!strcmp(mtok[0], "d")) {
            mat->op = yo__parse_float(mtok + 1);
        } else if (!strcmp(mtok[0], "Tr")) {
            mat->op = yo__parse_float(mtok + 1);
        } else if (!strcmp(mtok[0], "Ni")) {
            mat->ior = yo__parse_float(mtok + 1);
        } else if (!strcmp(mtok[0], "map_Ke")) {
            mat->ke_txt = mtok[1];
            mat->ke_txtid = yo__add_unique_texture(textures, mat->ke_txt);
        } else if (!strcmp(mtok[0], "map_Ka")) {
            mat->ka_txt = mtok[1];
            mat->ka_txtid = yo__add_unique_texture(textures, mat->ka_txt);
        } else if (!strcmp(mtok[0], "map_Kd")) {
            mat->kd_txt = mtok[1];
            mat->kd_txtid = yo__add_unique_texture(textures, mat->kd_txt);
        } else if (!strcmp(mtok[0], "map_Ks")) {
            mat->ks_txt = mtok[1];
            mat->ks_txtid = yo__add_unique_texture(textures, mat->ks_txt);
        } else if (!strcmp(mtok[0], "map_Kr")) {
            mat->kr_txt = mtok[1];
            mat->kr_txtid = yo__add_unique_texture(textures, mat->kr_txt);
        } else if (!strcmp(mtok[0], "map_Tr")) {
            mat->kt_txt = mtok[1];
            mat->kt_txtid = yo__add_unique_texture(textures, mat->kt_txt);
        } else if (!strcmp(mtok[0], "map_Ns")) {
            mat->ns_txt = mtok[1];
            mat->ns_txtid = yo__add_unique_texture(textures, mat->ns_txt);
        } else if (!strcmp(mtok[0], "map_d")) {
            mat->op_txt = mtok[1];
            mat->op_txtid = yo__add_unique_texture(textures, mat->op_txt);
        } else if (!strcmp(mtok[0], "map_Tr")) {
            mat->op_txt = mtok[1];
            mat->op_txtid = yo__add_unique_texture(textures, mat->op_txt);
        } else if (!strcmp(mtok[0], "map_Ni")) {
            mat->ior_txt = mtok[1];
            mat->ior_txtid = yo__add_unique_texture(textures, mat->ior_txt);
        } else if (!strcmp(mtok[0], "map_bump")) {
            mat->bump_txt = mtok[1];
            mat->bump_txtid = yo__add_unique_texture(textures, mat->bump_txt);
        } else if (!strcmp(mtok[0], "map_disp")) {
            mat->disp_txt = mtok[1];
            mat->disp_txtid = yo__add_unique_texture(textures, mat->disp_txt);
        } else {
            // printf("ignoring value for %s\n", mtok[0]);
        }
    }

    fclose(mfile);

    return true;
}

//
// Splits a path into component to get directory name
//
static inline void yo__split_path(const std::string& filename,
                                  std::string* dirname, std::string* basename,
                                  std::string* ext) {
    // walk till end keeping the position of '/', '\\' and '.'
    auto path_sep = -1, ext_sep = -1;
    for (auto i = 0; i < filename.length(); i++) {
        if (filename[i] == '/' || filename[i] == '\\') path_sep = i;
        if (filename[i] == '.') ext_sep = i;
    }

    // copy strings
    if (dirname) {
        if (path_sep >= 0) {
            *dirname = filename.substr(0, path_sep + 1);
        } else {
            *dirname = "";
        }
    }
    if (basename) {
        auto start = (path_sep >= 0) ? path_sep + 1 : 0;
        if (ext_sep >= 0) {
            *basename = filename.substr(start, ext_sep);
        } else {
            *basename = filename.substr(start);
        }
    }
    if (ext) {
        if (ext_sep) {
            *ext = filename.substr(ext_sep);
        } else {
            *ext = "";
        }
    }
}

//
// Loads an OBJ file
//
YGL_API yo_scene* yo_load_obj(const std::string& filename, bool triangulate,
                              bool ext) {
    // prepare scene
    yo_scene* scene = new yo_scene();

    // vertex scene
    yo__vertdata obj_vert;

    // current scene objects
    std::vector<yo_shape> shapes;
    std::vector<yo_material> materials;
    std::vector<yo_texture> textures;
    std::vector<yo_camera> cameras;
    std::vector<yo_env> envs;

    // current shape scene
    yo__vhash* vhash = new yo__vhash();
    ym_string name, matname, groupname;
    ym_affine3f xform = ym_identity_affine3f;
    yo__elemdata elem;
    yo__vertdata vert;

    // start
    FILE* file = fopen(filename.c_str(), "rt");
    if (!file) return 0;

    // foreach line, splits the line by whitespaces and parses the data
    // directly in the current shape, emitting shapes when either name,
    // material name, group name or shape element type changes
    char line[4096];
    char* tok[1024];
    int linenum = 0;
    while (fgets(line, 4096, file)) {
        linenum += 1;
        int ntok = yo__splitws(line, tok, 1024);

        if (!ntok) {
            continue;
        } else if (tok[0][0] == '#') {
            continue;
        } else if (!strcmp(tok[0], "v")) {
            obj_vert.pos.push_back(yo__parse_float3(tok + 1));
        } else if (!strcmp(tok[0], "vt")) {
            obj_vert.texcoord.push_back(yo__parse_float2(tok + 1));
        } else if (!strcmp(tok[0], "vn")) {
            obj_vert.norm.push_back(yo__parse_float3(tok + 1));
        } else if (!strcmp(tok[0], "vc")) {
            if (ext) {
                obj_vert.norm.push_back(yo__parse_float3(tok + 1));
            }
        } else if (!strcmp(tok[0], "vr")) {
            if (ext) {
                obj_vert.radius.push_back(yo__parse_float(tok + 1));
            }
        } else if (!strcmp(tok[0], "xf")) {
            if (ext) {
                xform = yo__parse_affine3f(tok + 1);
            }
        } else if (!strcmp(tok[0], "c")) {
            if (ext) {
                yo__add_shape(shapes, materials, name, matname, groupname,
                              xform, elem, vert, vhash);
                yo__vert from = yo__parse_vert(tok[1], vhash, obj_vert);
                yo__vert to = yo__parse_vert(tok[2], vhash, obj_vert);
                yo__add_camera(cameras, name, from, to, obj_vert, vhash);
                name = {};
                matname = {};
                xform = ym_identity_affine3f;
            }
        } else if (!strcmp(tok[0], "e")) {
            if (ext) {
                yo__add_shape(shapes, materials, name, matname, groupname,
                              xform, elem, vert, vhash);
                yo__vert from = yo__parse_vert(tok[1], vhash, obj_vert);
                yo__vert to = yo__parse_vert(tok[2], vhash, obj_vert);
                yo__add_env(envs, name, matname, from, to, obj_vert, vhash);
                name = {};
                matname = {};
                xform = ym_identity_affine3f;
            }
        } else if (!strcmp(tok[0], "f") && !triangulate) {
            if (elem.etype != yo_etype_polygon) {
                yo__add_shape(shapes, materials, name, matname, groupname,
                              xform, elem, vert, vhash);
            }
            elem.etype = yo_etype_polygon;
            elem.elem.push_back(ntok - 1);
            for (int t = 1; t < ntok; t++) {
                yo__vert v = yo__parse_vert(tok[t], vhash, obj_vert);
                yo__add_shape_vert(&vert, v, obj_vert);
                elem.elem.push_back(v.vid);
            }
        } else if (!strcmp(tok[0], "f") && triangulate) {
            if (elem.etype != yo_etype_triangle) {
                yo__add_shape(shapes, materials, name, matname, groupname,
                              xform, elem, vert, vhash);
            }
            elem.etype = yo_etype_triangle;
            int vi0 = 0;
            for (int t = 1; t < ntok; t++) {
                yo__vert v = yo__parse_vert(tok[t], vhash, obj_vert);
                yo__add_shape_vert(&vert, v, obj_vert);
                if (t == 1) vi0 = v.vid;
                if (t > 3) {
                    int vil = elem.elem[elem.elem.size() - 1];
                    elem.elem.push_back(vi0);
                    elem.elem.push_back(vil);
                }
                elem.elem.push_back(v.vid);
            }
        } else if (!strcmp(tok[0], "l") && !triangulate) {
            if (elem.etype != yo_etype_polyline) {
                yo__add_shape(shapes, materials, name, matname, groupname,
                              xform, elem, vert, vhash);
            }
            elem.etype = yo_etype_polyline;
            elem.elem.push_back(ntok - 1);
            for (int t = 1; t < ntok; t++) {
                yo__vert v = yo__parse_vert(tok[t], vhash, obj_vert);
                yo__add_shape_vert(&vert, v, obj_vert);
                elem.elem.push_back(v.vid);
            }
        } else if (!strcmp(tok[0], "l") && triangulate) {
            if (elem.etype != yo_etype_line) {
                yo__add_shape(shapes, materials, name, matname, groupname,
                              xform, elem, vert, vhash);
            }
            elem.etype = yo_etype_line;
            for (int t = 1; t < ntok; t++) {
                yo__vert v = yo__parse_vert(tok[t], vhash, obj_vert);
                yo__add_shape_vert(&vert, v, obj_vert);
                if (t > 2) {
                    int vil = elem.elem[elem.elem.size() - 1];
                    elem.elem.push_back(vil);
                }
                elem.elem.push_back(v.vid);
            }
        } else if (!strcmp(tok[0], "p")) {
            if (elem.etype != yo_etype_point) {
                yo__add_shape(shapes, materials, name, matname, groupname,
                              xform, elem, vert, vhash);
            }
            elem.etype = yo_etype_point;
            for (int t = 1; t < ntok; t++) {
                yo__vert v = yo__parse_vert(tok[t], vhash, obj_vert);
                yo__add_shape_vert(&vert, v, obj_vert);
                elem.elem.push_back(v.vid);
            }
        } else if (!strcmp(tok[0], "o")) {
            yo__add_shape(shapes, materials, name, matname, groupname, xform,
                          elem, vert, vhash);
            name = ym_string((ntok > 1) ? tok[1] : 0);
            matname = {};
            groupname = {};
            xform = ym_identity_affine3f;
        } else if (!strcmp(tok[0], "g")) {
            yo__add_shape(shapes, materials, name, matname, groupname, xform,
                          elem, vert, vhash);
            groupname = ym_string((ntok > 1) ? tok[1] : 0);
        } else if (!strcmp(tok[0], "usemtl")) {
            yo__add_shape(shapes, materials, name, matname, groupname, xform,
                          elem, vert, vhash);
            matname = ym_string((ntok > 1) ? tok[1] : 0);
        } else if (!strcmp(tok[0], "mtllib")) {
            std::string dirname;
            yo__split_path(filename, &dirname, nullptr, nullptr);
            auto mfilename = dirname + tok[1];
            if (!yo__load_mtl(materials, textures, mfilename)) return 0;
        } else {
            // TODO: explicit skips
        }
    }

    // flush and cleanup empty shape if necessary
    yo__add_shape(shapes, materials, name, matname, groupname, xform, elem,
                  vert, vhash);

    // close file
    fclose(file);

    // add data to scene
    scene->shapes = shapes;
    scene->materials = materials;
    scene->textures = textures;
    scene->cameras = cameras;
    scene->envs = envs;

    return scene;
}

// -----------------------------------------------------------------------------
// OBJ SAVING
// -----------------------------------------------------------------------------

// write one float prepended by a string
static inline void yo__fwrite_float(FILE* file, const char* str, float v) {
    fprintf(file, "%s %.6g\n", str, v);
}

// write one float prepended by a string
static inline void yo__fwrite_int(FILE* file, const char* str, int v) {
    fprintf(file, "%s %d\n", str, v);
}

// write two floats prepended by a string
static inline void yo__fwrite_float2(FILE* file, const char* str,
                                     const ym_vec2f& v) {
    fprintf(file, "%s %.6g %.6g\n", str, v[0], v[1]);
}

// write three floats prepended by a string
static inline void yo__fwrite_float3(FILE* file, const char* str,
                                     const ym_vec3f& v) {
    fprintf(file, "%s %.6g %.6g %.6g\n", str, v[0], v[1], v[2]);
}

// write 16 floats prepended by a string
static inline void yo__fwrite_float12(FILE* file, const char* str,
                                      const ym_affine3f& v) {
    const float* vf = (float*)&v;
    fprintf(file, "%s", str);
    for (int i = 0; i < 12; i++) fprintf(file, " %.6g", vf[i]);
    fprintf(file, "\n");
}

// write a string prepended by another if the string is not NULL
static inline void yo__fwrite_str(FILE* file, const char* str,
                                  const ym_string& s, bool force = false) {
    if (!s.empty() || force) fprintf(file, "%s %s\n", str, s.c_str());
}

//
// save MTL file
//
static inline bool yo__save_mtl(const std::string& filename,
                                const yo_scene* scene) {
    // TODO: failure
    FILE* mfile = fopen(filename.c_str(), "wt");
    if (!mfile) return false;

    // for each material, dump all the values
    for (int i = 0; i < scene->materials.size(); i++) {
        const yo_material* mat = &scene->materials[i];
        yo__fwrite_str(mfile, "newmtl", mat->name, true);
        yo__fwrite_int(mfile, "  illum", mat->illum);
        yo__fwrite_float3(mfile, "  Ke", mat->ke);
        yo__fwrite_float3(mfile, "  Kd", mat->kd);
        yo__fwrite_float3(mfile, "  Ks", mat->ks);
        yo__fwrite_float3(mfile, "  Kr", mat->kr);
        yo__fwrite_float3(mfile, "  Kt", mat->kt);
        yo__fwrite_float(mfile, "  Ns", mat->ns);
        yo__fwrite_float(mfile, "  d", mat->op);
        yo__fwrite_float(mfile, "  Ni", mat->ior);
        yo__fwrite_str(mfile, "  map_Ke", mat->ke_txt);
        yo__fwrite_str(mfile, "  map_Kd", mat->kd_txt);
        yo__fwrite_str(mfile, "  map_Ks", mat->ks_txt);
        yo__fwrite_str(mfile, "  map_Kr", mat->kr_txt);
        yo__fwrite_str(mfile, "  map_Kt", mat->kt_txt);
        yo__fwrite_str(mfile, "  map_Ns", mat->ns_txt);
        yo__fwrite_str(mfile, "  map_d", mat->op_txt);
        yo__fwrite_str(mfile, "  map_Ni", mat->ior_txt);
        yo__fwrite_str(mfile, "  map_bump", mat->bump_txt);
        yo__fwrite_str(mfile, "  map_disp", mat->disp_txt);
        fprintf(mfile, "\n");
    }

    fclose(mfile);

    return true;
}

// write an OBJ vertex triplet using only the indices that are active
static inline void yo__fwrite_objverts(FILE* file, const char* str, int nv,
                                       const int* vid, yo__vert voffset,
                                       int nto_write, yo__vert to_write) {
    fprintf(file, "%s", str);
    for (int v = 0; v < nv; v++) {
        for (int i = 0; i < nto_write; i++) {
            if ((&to_write.pos)[i])
                fprintf(file, "%c%d", ((i == 0) ? ' ' : '/'),
                        (&voffset.pos)[i] + vid[v]);
            else
                fprintf(file, "%c", '/');
        }
    }
    fprintf(file, "\n");
}

//
// save OBJ
//
YGL_API bool yo_save_obj(const std::string& filename, const yo_scene* scene,
                         bool ext) {
    std::string dirname, mfilename;
    yo__split_path(filename.c_str(), &dirname, &mfilename, nullptr);
    mfilename += ".mtl";

    // write material file
    if (!scene->materials.empty()) {
        std::string fullname;
        if (dirname.length())
            fullname = dirname + mfilename;
        else
            fullname = mfilename;
        if (!yo__save_mtl(fullname, scene)) return false;
    }

    FILE* file = fopen(filename.c_str(), "wt");
    if (!file) return false;

    if (!scene->materials.empty()) {
        fprintf(file, "mtllib %s\n", mfilename.c_str());
    }

    yo__vert voffset = {1, 1, 1, 1, 1};

    // write cameras and environments if extensions are enabled
    if (ext) {
        for (int cid = 0; cid < scene->cameras.size(); cid++) {
            const yo_camera* cam = &scene->cameras[cid];
            yo__fwrite_str(file, "o", cam->name);
            yo__fwrite_float3(file, "v", cam->from);
            yo__fwrite_float3(file, "v", cam->to);
            yo__fwrite_float3(file, "vn", cam->up);
            yo__fwrite_float3(file, "vn", cam->up);
            yo__fwrite_float2(file, "vt", {cam->aperture, cam->aperture});
            yo__fwrite_float2(file, "vt", {cam->width, cam->height});
            int vid[2] = {0, 1};
            yo__fwrite_objverts(file, "c", 2, vid, voffset, 3,
                                yo__vert{1, 1, 1, 0, 0, 0});
            voffset.pos += 2;
            voffset.norm += 2;
            voffset.texcoord += 2;
        }
        for (int cid = 0; cid < scene->envs.size(); cid++) {
            const yo_env* env = &scene->envs[cid];
            yo__fwrite_str(file, "o", env->name);
            yo__fwrite_str(file, "usemtl", env->matname);
            yo__fwrite_float3(file, "v", env->from);
            yo__fwrite_float3(file, "v", env->to);
            yo__fwrite_float3(file, "vn", env->up);
            yo__fwrite_float3(file, "vn", env->up);
            int vid[2] = {0, 1};
            yo__fwrite_float2(file, "vt", ym_zero2f);
            yo__fwrite_float2(file, "vt", ym_zero2f);
            yo__fwrite_objverts(file, "e", 2, vid, voffset, 3,
                                yo__vert{1, 1, 1, 0, 0, 0});
            voffset.pos += 2;
            voffset.norm += 2;
            voffset.texcoord += 2;
        }
    }

    // write all shape data
    for (int sid = 0; sid < scene->shapes.size(); sid++) {
        const yo_shape* shape = &scene->shapes[sid];
        // shape header (name, material)
        yo__fwrite_str(file, "o", shape->name);
        yo__fwrite_str(file, "usemtl", shape->matname);
        if (ext && shape->xformed) yo__fwrite_float12(file, "xf", shape->xform);

        // shape vertices
        yo__vert vto_write = {(!shape->pos.empty()) ? 1 : 0,
                              (!shape->texcoord.empty()) ? 1 : 0,
                              (!shape->norm.empty()) ? 1 : 0,
                              ((ext && !shape->color.empty()) ? 1 : 0),
                              ((ext && !shape->radius.empty()) ? 1 : 0),
                              0};
        int nto_write = 0;
        for (int i = 0; i < ((ext) ? 5 : 3); i++)
            nto_write = (&vto_write.pos)[i] ? i + 1 : nto_write;
        for (int j = 0; j < shape->nverts; j++) {
            yo__fwrite_float3(file, "v ", shape->pos[j]);
            if (vto_write.norm) yo__fwrite_float3(file, "vn", shape->norm[j]);
            if (vto_write.texcoord)
                yo__fwrite_float2(file, "vt", shape->texcoord[j]);
            if (ext && vto_write.color)
                yo__fwrite_float3(file, "vc", shape->color[j]);
            if (ext && vto_write.radius)
                yo__fwrite_float(file, "vr", shape->radius[j]);
        }

        // shape elements
        switch (shape->etype) {
            case yo_etype_point:
            case yo_etype_line:
            case yo_etype_triangle: {
                const char* labels[4] = {0, "p", "l", "f"};
                int esize = shape->etype;
                const char* label = labels[shape->etype];
                for (int j = 0; j < shape->nelems; j++) {
                    const int* f = &shape->elem[j * esize];
                    yo__fwrite_objverts(file, label, esize, f, voffset,
                                        nto_write, vto_write);
                }
            } break;
            case yo_etype_polyline:
            case yo_etype_polygon: {
                const char* label =
                    (shape->etype == yo_etype_polyline) ? "l" : "f";
                for (int j = 0, e = 0; j < shape->nelems; j++) {
                    int esize = shape->elem[e++];
                    const int* f = &shape->elem[e];
                    yo__fwrite_objverts(file, label, esize, f, voffset,
                                        nto_write, vto_write);
                }
            } break;
            default: { assert(false); } break;
        }
        for (int i = 0; i < 5; i++)
            (&voffset.pos)[i] += ((&vto_write.pos)[i]) ? shape->nverts : 0;
    }

    fclose(file);

    return true;
}

// -----------------------------------------------------------------------------
// BINARY DUMP LOADING
// -----------------------------------------------------------------------------

// magic code fron binary dump
#define yo__binmagic 0xaf45e782

// binary dump values
template <typename T>
static inline bool yo__fread_binvalue(FILE* file, T* v) {
    return fread(v, sizeof(T), 1, file) == 1;
}

// binary dump vector of values
template <typename T>
static inline bool yo__fread_binvector(FILE* file, std::vector<T>* v) {
    int num = 0;
    if (fread(&num, sizeof(int), 1, file) != 1) return false;
    v->resize(num);
    if (fread(v->data(), sizeof(T), num, file) != num) return false;
    return true;
}

// binary dump strings
static inline bool yo__fread_binstr(FILE* file, ym_string* s) {
    char buf[4096];
    int num = 0;
    if (fread(&num, sizeof(int), 1, file) != 1) return false;
    assert(num < sizeof(buf) - 1);
    if (num) {
        if (fread(buf, 1, num, file) != num) return false;
        *s = buf;
    } else {
        *s = "";
    }
    return true;
}

//
// binary dump OBJ (note that material data is dumped in the same file)
//
YGL_API yo_scene* yo_load_objbin(const std::string& filename, bool ext) {
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) return 0;

    // TODO: ids

    int magic = 0;
    yo__fread_binvalue(file, &magic);
    if (magic != yo__binmagic) return 0;

    yo_scene* scene = new yo_scene();

    int ncameras = 0;
    yo__fread_binvalue(file, &ncameras);
    scene->cameras.resize(ncameras);
    for (int i = 0; i < ncameras; i++) {
        yo_camera* cam = &scene->cameras[i];
        yo__fread_binstr(file, &cam->name);
        yo__fread_binvalue(file, &cam->from);
        yo__fread_binvalue(file, &cam->to);
        yo__fread_binvalue(file, &cam->up);
        yo__fread_binvalue(file, &cam->width);
        yo__fread_binvalue(file, &cam->height);
        yo__fread_binvalue(file, &cam->aperture);
    }

    int nenvs = 0;
    yo__fread_binvalue(file, &nenvs);
    scene->envs.resize(nenvs);
    for (int i = 0; i < nenvs; i++) {
        yo_env* env = &scene->envs[i];
        yo__fread_binstr(file, &env->name);
        yo__fread_binstr(file, &env->matname);
        yo__fread_binvalue(file, &env->from);
        yo__fread_binvalue(file, &env->to);
        yo__fread_binvalue(file, &env->up);
    }

    if (!ext) {
        scene->cameras.clear();
        scene->envs.clear();
    }

    int nmaterials = 0;
    yo__fread_binvalue(file, &nmaterials);
    scene->materials.resize(nmaterials);
    for (int i = 0; i < nmaterials; i++) {
        yo_material* mat = &scene->materials[i];
        yo__fread_binstr(file, &mat->name);
        yo__fread_binvalue(file, &mat->illum);
        yo__fread_binvalue(file, &mat->ke);
        yo__fread_binvalue(file, &mat->ka);
        yo__fread_binvalue(file, &mat->kd);
        yo__fread_binvalue(file, &mat->ks);
        yo__fread_binvalue(file, &mat->kr);
        yo__fread_binvalue(file, &mat->kt);
        yo__fread_binvalue(file, &mat->ns);
        yo__fread_binvalue(file, &mat->ior);
        yo__fread_binvalue(file, &mat->op);
        yo__fread_binstr(file, &mat->ke_txt);
        yo__fread_binstr(file, &mat->ka_txt);
        yo__fread_binstr(file, &mat->kd_txt);
        yo__fread_binstr(file, &mat->ks_txt);
        yo__fread_binstr(file, &mat->kr_txt);
        yo__fread_binstr(file, &mat->kt_txt);
        yo__fread_binstr(file, &mat->ns_txt);
        yo__fread_binstr(file, &mat->op_txt);
        yo__fread_binstr(file, &mat->ior_txt);
        yo__fread_binstr(file, &mat->bump_txt);
        yo__fread_binstr(file, &mat->disp_txt);
        mat->ke_txtid = yo__add_unique_texture(scene->textures, mat->ke_txt);
        mat->ka_txtid = yo__add_unique_texture(scene->textures, mat->ka_txt);
        mat->kd_txtid = yo__add_unique_texture(scene->textures, mat->kd_txt);
        mat->ks_txtid = yo__add_unique_texture(scene->textures, mat->ks_txt);
        mat->kr_txtid = yo__add_unique_texture(scene->textures, mat->kr_txt);
        mat->kt_txtid = yo__add_unique_texture(scene->textures, mat->kt_txt);
        mat->ns_txtid = yo__add_unique_texture(scene->textures, mat->ns_txt);
        mat->op_txtid = yo__add_unique_texture(scene->textures, mat->op_txt);
        mat->ior_txtid = yo__add_unique_texture(scene->textures, mat->ior_txt);
        mat->bump_txtid =
            yo__add_unique_texture(scene->textures, mat->bump_txt);
        mat->disp_txtid =
            yo__add_unique_texture(scene->textures, mat->disp_txt);
    }

    int nshapes = 0;
    yo__fread_binvalue(file, &nshapes);
    scene->shapes.resize(nshapes);
    for (int i = 0; i < nshapes; i++) {
        yo_shape* shape = &scene->shapes[i];
        yo__fread_binstr(file, &shape->name);
        yo__fread_binstr(file, &shape->groupname);
        yo__fread_binstr(file, &shape->matname);
        yo__fread_binvalue(file, &shape->nelems);
        yo__fread_binvector(file, &shape->elem);
        yo__fread_binvalue(file, &shape->etype);
        yo__fread_binvalue(file, &shape->nverts);
        yo__fread_binvector(file, &shape->pos);
        yo__fread_binvector(file, &shape->norm);
        yo__fread_binvector(file, &shape->texcoord);
        yo__fread_binvector(file, &shape->color);
        yo__fread_binvector(file, &shape->radius);
        if (ext) {
            shape->color.clear();
            shape->radius.clear();
        }
        shape->matid = -1;
        for (int j = 0; j < scene->materials.size() && shape->matid < 0; j++) {
            if (shape->matname == scene->materials[j].name) shape->matid = j;
        }
    }

    fclose(file);

    return scene;
}

// -----------------------------------------------------------------------------
// BINARY DUMP SAVING
// -----------------------------------------------------------------------------

// binary dump values
template <typename T>
static inline bool yo__fwrite_binvalue(FILE* file, const T& v) {
    return fwrite(&v, sizeof(T), 1, file) == 1;
}

// binary dump vector of values
template <typename T>
static inline bool yo__fwrite_binvector(FILE* file, const std::vector<T>& v) {
    int num = (int)v.size();
    if (fwrite(&num, sizeof(int), 1, file) != 1) return false;
    if (fwrite(v.data(), sizeof(T), num, file) != num) return false;
    return true;
}

// binary dump strings
static inline bool yo__fwrite_binstr(FILE* file, const ym_string& s) {
    int num = (int)s.length() + 1;
    if (fwrite(&num, sizeof(int), 1, file) != 1) return false;
    if (fwrite(s.c_str(), 1, num, file) != num) return false;
    return true;
}

//
// load binary obj dump
//
YGL_API bool yo_save_objbin(const std::string& filename, const yo_scene* scene,
                            bool ext) {
    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) return false;

    int magic = yo__binmagic;
    yo__fwrite_binvalue(file, magic);

    if (ext) {
        int ncameras = (int)scene->cameras.size();
        yo__fwrite_binvalue(file, ncameras);
        for (int i = 0; i < ncameras; i++) {
            const yo_camera* cam = &scene->cameras[i];
            yo__fwrite_binstr(file, cam->name);
            yo__fwrite_binvalue(file, cam->from);
            yo__fwrite_binvalue(file, cam->to);
            yo__fwrite_binvalue(file, cam->up);
            yo__fwrite_binvalue(file, cam->width);
            yo__fwrite_binvalue(file, cam->height);
            yo__fwrite_binvalue(file, cam->aperture);
        }
        int nenvs = (int)scene->envs.size();
        yo__fwrite_binvalue(file, nenvs);
        for (int i = 0; i < nenvs; i++) {
            const yo_env* env = &scene->envs[i];
            yo__fwrite_binstr(file, env->name);
            yo__fwrite_binstr(file, env->matname);
            yo__fwrite_binvalue(file, env->from);
            yo__fwrite_binvalue(file, env->to);
            yo__fwrite_binvalue(file, env->up);
        }
    } else {
        int zero = 0;
        yo__fwrite_binvalue(file, zero);
        yo__fwrite_binvalue(file, zero);
    }

    int nmaterials = (int)scene->materials.size();
    yo__fwrite_binvalue(file, nmaterials);
    for (int i = 0; i < nmaterials; i++) {
        const yo_material* mat = &scene->materials[i];
        yo__fwrite_binstr(file, mat->name);
        yo__fwrite_binvalue(file, mat->illum);
        yo__fwrite_binvalue(file, mat->ke);
        yo__fwrite_binvalue(file, mat->ka);
        yo__fwrite_binvalue(file, mat->kd);
        yo__fwrite_binvalue(file, mat->ks);
        yo__fwrite_binvalue(file, mat->kr);
        yo__fwrite_binvalue(file, mat->kt);
        yo__fwrite_binvalue(file, mat->ns);
        yo__fwrite_binvalue(file, mat->ior);
        yo__fwrite_binvalue(file, mat->op);
        yo__fwrite_binstr(file, mat->ke_txt);
        yo__fwrite_binstr(file, mat->ka_txt);
        yo__fwrite_binstr(file, mat->kd_txt);
        yo__fwrite_binstr(file, mat->ks_txt);
        yo__fwrite_binstr(file, mat->kr_txt);
        yo__fwrite_binstr(file, mat->kt_txt);
        yo__fwrite_binstr(file, mat->ns_txt);
        yo__fwrite_binstr(file, mat->op_txt);
        yo__fwrite_binstr(file, mat->ior_txt);
        yo__fwrite_binstr(file, mat->bump_txt);
        yo__fwrite_binstr(file, mat->disp_txt);
    }

    int nshapes = (int)scene->shapes.size();
    yo__fwrite_binvalue(file, nshapes);
    for (int i = 0; i < nshapes; i++) {
        const yo_shape* shape = &scene->shapes[i];
        yo__fwrite_binstr(file, shape->name);
        yo__fwrite_binstr(file, shape->groupname);
        yo__fwrite_binstr(file, shape->matname);
        yo__fwrite_binvalue(file, shape->nelems);
        yo__fwrite_binvector(file, shape->elem);
        yo__fwrite_binvalue(file, shape->etype);
        yo__fwrite_binvalue(file, shape->nverts);
        yo__fwrite_binvector(file, shape->pos);
        yo__fwrite_binvector(file, shape->norm);
        yo__fwrite_binvector(file, shape->texcoord);
        if (ext) {
            yo__fwrite_binvector(file, shape->color);
            yo__fwrite_binvector(file, shape->radius);
        } else {
            std::vector<float> r;
            std::vector<ym_vec3f> c;
            yo__fwrite_binvector(file, c);
            yo__fwrite_binvector(file, r);
        }
    }

    fclose(file);

    return true;
}

// -----------------------------------------------------------------------------
// TEXTURE HANDLING
// -----------------------------------------------------------------------------

//
// handles texture loading using stb_image.h
//
#ifndef YO_NOIMG

// stb_images causes a lot of warning and issues when including,
// try to reduce them using pragmas
#ifndef STBI_INCLUDE_STB_IMAGE_H

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC

#ifndef _WIN32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#endif

#include "../yocto/ext/stb_image.h"

#ifndef _WIN32
#pragma GCC diagnostic pop
#endif

#endif

//
// load texture data
//
YGL_API void yo_load_textures(yo_scene* scene, const std::string& filename,
                              int req_comp) {
    stbi_set_flip_vertically_on_load(1);
    auto dirname = std::string();
    yo__split_path(filename, &dirname, nullptr, nullptr);
    for (int i = 0; i < scene->textures.size(); i++) {
        auto fullname = dirname + scene->textures[i].path;
        float* d = stbi_loadf(fullname.c_str(), &scene->textures[i].width,
                              &scene->textures[i].height,
                              &scene->textures[i].ncomp, req_comp);
        scene->textures[i].pixels = std::vector<float>(
            d, d +
                   scene->textures[i].width * scene->textures[i].height *
                       scene->textures[i].ncomp);
        free(d);
    }
    stbi_set_flip_vertically_on_load(0);
}

#endif

// -----------------------------------------------------------------------------
// IMPLEMENTATION OF DEPRECATED INTERFACE
// -----------------------------------------------------------------------------

//
// Wrapper. See above.
//
YGL_API yo_scene* yo_load_obj(const char* filename, bool triangulate,
                              bool ext) {
    return yo_load_obj(std::string(filename), triangulate, ext);
}

//
// Wrapper. See above.
//
YGL_API yo_scene* yo_load_objbin(const char* filename, bool ext) {
    return yo_load_objbin(std::string(filename), ext);
}

//
// Wrapper. See above.
//
YGL_API bool yo_save_obj(const char* filename, const yo_scene* scene,
                         bool ext) {
    return yo_save_obj(std::string(filename), scene, ext);
}

//
// Wrapper. See above.
//
YGL_API bool yo_save_objbin(const char* filename, const yo_scene* scene,
                            bool ext) {
    return yo_save_objbin(std::string(filename), scene, ext);
}

//
// Wrapper. See above.
//
#ifndef YO_NOIMG
YGL_API void yo_load_textures(yo_scene* scene, const char* filename,
                              int req_comp) {
    yo_load_textures(scene, std::string(filename), req_comp);
}
#endif

#endif

#endif
