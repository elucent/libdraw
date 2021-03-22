#include "model.h"
#include "lib/util/io.h"

static vector<Buffer> buffers;

Buffer::Buffer():
    dirty(true) {
    glGenBuffers(1, &vbuf);
    glGenBuffers(1, &cbuf);
    glGenBuffers(1, &nbuf);
    glGenBuffers(1, &tbuf);
    glGenBuffers(1, &sbuf);
}

void Buffer::bake() {
    dirty = false;

    glBindBuffer(GL_ARRAY_BUFFER, vbuf);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), &verts[0], GL_DYNAMIC_DRAW);
    
    glBindBuffer(GL_ARRAY_BUFFER, cbuf);
    glBufferData(GL_ARRAY_BUFFER, cols.size() * sizeof(float), &cols[0], GL_DYNAMIC_DRAW);
    
    glBindBuffer(GL_ARRAY_BUFFER, nbuf);
    glBufferData(GL_ARRAY_BUFFER, norms.size() * sizeof(float), &norms[0], GL_DYNAMIC_DRAW);
    
    glBindBuffer(GL_ARRAY_BUFFER, tbuf);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(float), &uvs[0], GL_DYNAMIC_DRAW);
    
    glBindBuffer(GL_ARRAY_BUFFER, sbuf);
    glBufferData(GL_ARRAY_BUFFER, sprs.size() * sizeof(float), &sprs[0], GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (verts.size() / 3 != cols.size() / 4 || verts.size() / 3 != norms.size() / 3
        || verts.size() / 3 != uvs.size() / 2 || verts.size() / 3 != sprs.size() / 4) {
        println("Incorrect buffer sizes.");
    }
}

bool Buffer::empty() const {
    return verts.size() == 0;
}

void Buffer::reset() {
    verts.clear();
    cols.clear();
    norms.clear();
    uvs.clear();
    sprs.clear();
    dirty = true;
}

void Buffer::draw() {
    if (dirty) bake();
    int nverts = verts.size() / 3;

    for (int i = 0; i < 5; i ++) glEnableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, vbuf);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, cbuf);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, nbuf);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, tbuf);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, sbuf);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    glDrawArrays(GL_TRIANGLES, 0, nverts);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    for (int i = 0; i < 5; i ++) glDisableVertexAttribArray(i);
}

void Buffer::takefrom(const Buffer& buf, float dx, float dy, float dz, float r, float g, float b, float a) {
    dirty = true;
    int i = 0;
    float ds[3] = { dx, dy, dz };
    float color[4] = { r, g, b, a };
    for (float f : buf.verts) verts.push(f + ds[i ++ % 3]);
    i = 0;
    for (float f : buf.cols) cols.push(f + color[i ++ % 4]);
    for (float f : buf.norms) norms.push(f);
    for (float f : buf.uvs) uvs.push(f);
    for (float f : buf.sprs) sprs.push(f);
}

void Buffer::pos(float x, float y, float z) {
    dirty = true;
    verts.push(x);
    verts.push(y);
    verts.push(z);
}

void Buffer::col(float r, float g, float b, float a) {
    dirty = true;
    cols.push(r);
    cols.push(g);
    cols.push(b);
    cols.push(a);
}  

void Buffer::uv(float u, float v) {
    dirty = true;
    uvs.push(u);
    uvs.push(v);
}

void Buffer::spr(float x, float y, float w, float h) {
    dirty = true;
    sprs.push(x);
    sprs.push(y);
    sprs.push(w);
    sprs.push(h);
}

void Buffer::norm(float x, float y, float z) {
    dirty = true;
    norms.push(x);
    norms.push(y);
    norms.push(z);
}

Buffer& findbuf(Model handle) {
    return buffers[handle];
}

Model init_render_buffer() {
    buffers.push(Buffer());
    return 0;
}

Model createmodel() {
    buffers.push(Buffer());
    return buffers.size() - 1;
}