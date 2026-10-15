#include "gl_drawlist_renderer.hpp"
#include <algorithm>
#include <cstddef>
#include <limits>
using namespace xui;
namespace {
	template<class T> bool load(T& p, const char* n) { p = reinterpret_cast<T>(glfwGetProcAddress(n)); return p != nullptr; }
	const char* vs = R"GLSL(#version 120
attribute vec2 p,uv; 
attribute vec4 c0,c1; 
attribute vec2 halfSize; 
attribute vec4 radii; 
attribute vec2 aux;
attribute float border,prim,flags; 
uniform vec2 viewport;
varying vec2 P,U,H,A; 
varying vec4 C0,C1,R; 
varying float B,T,F;
void main(){
	gl_Position=vec4(p.x*2.0/viewport.x-1.0,1.0-p.y*2.0/viewport.y,0.0,1.0);
	P=p;
	U=uv;
	C0=c0.wzyx;
	C1=c1.wzyx;
	H=halfSize;
	R=radii*.25;
	A=aux;
	B=border*.25;
	T=prim;
	F=flags;
})GLSL";
	const char* fs = R"GLSL(#version 120
uniform sampler2D tex; 
uniform int hasTex;
varying vec2 P,U,H,A; 
varying vec4 C0,C1,R; 
varying float B,T,F;
float cov(float d){
	return clamp(.5-d,0.0,1.0);
}
float fall(float d,float b){
	if(b<=0.0)return cov(d);
	float a=clamp(.5-d/b,0.0,1.0);
	return a*a*(3.0-2.0*a);
}
float rr(vec2 p,vec2 h,vec4 r){
	float q=p.x<0.0?(p.y<0.0?r.x:r.w):(p.y<0.0?r.y:r.z);
	vec2 v=abs(p)-h+q;
	return length(max(v,0.0))+min(max(v.x,v.y),0.0)-q;
}
void main(){
	vec4 c=C0;
	float a=0.0;
	vec3 rgb=vec3(0.0);
	if(T<.5){
		float d=rr(U,H,R),o=cov(d);
		if(mod(F,2.0)>0.5){
			float den=abs(H.x*A.x)+abs(H.y*A.y);
			c=mix(C0,C1,den>0.0?clamp(.5+.5*dot(U,A)/den,0.0,1.0):0.0);
			a=c.a*o;
		}
		else if(B<=0.0)a=C0.a*o;
		else{
			float inr=cov(d+B);
			vec4 ring=C1;
			if(mod(floor(F/2.0),2.0)>0.5){
				float ex=H.x-abs(U.x),ey=H.y-abs(U.y);
				float s=(ey<ex?U.x+H.x:U.y+H.y)/max(A.x,.001);
				if(fract(s)>=.5)ring=vec4(0.0);
			}
			c=C0*inr+ring*(1.0-inr);
			a=c.a*o;
		}
		rgb=c.rgb*a;
	}
	else if(T<1.5){
		float d=rr(U,H,R);
		a=C0.a*(mod(floor(F/4.0),2.0)>.5?(1.0-fall(rr(U-A,H,R),B))*cov(d):fall(d,B));
		rgb=C0.rgb*a;
	}
	else if(T<2.5){
		a=C0.a*(hasTex!=0?texture2D(tex,U).a:0.0);
		rgb=C0.rgb*a;
	}
	else if(T<3.5){
		vec4 t=hasTex!=0?texture2D(tex,U):vec4(1.0);
		a=t.a*C0.a*cov(rr(P-A,H,R));
		rgb=t.rgb*C0.rgb*a;
	}
	else{
		vec2 d=2.0*H,ab=-H;
		float t=dot(d,d)>0.0?clamp(dot(U-ab,d)/dot(d,d),0.0,1.0):0.0;
		float dist=length(U-(ab+d*t))-R.x;
		c=mod(F,2.0)>.5?mix(C0,C1,t):C0;a=c.a*cov(dist);
		rgb=c.rgb*a;
	}
	gl_FragColor=vec4(rgb,a);
})GLSL";
}
bool GlDrawListRenderer::loadFunctions() {
	return
		load(m_createShader, "glCreateShader") &&
		load(m_shaderSource, "glShaderSource") &&
		load(m_compileShader, "glCompileShader") &&
		load(m_getShaderiv, "glGetShaderiv") &&
		load(m_deleteShader, "glDeleteShader") &&
		load(m_createProgram, "glCreateProgram") &&
		load(m_attachShader, "glAttachShader") &&
		load(m_bindAttribLocation, "glBindAttribLocation") &&
		load(m_linkProgram, "glLinkProgram") &&
		load(m_getProgramiv, "glGetProgramiv") &&
		load(m_deleteProgram, "glDeleteProgram") &&
		load(m_useProgram, "glUseProgram") &&
		load(m_getUniformLocation, "glGetUniformLocation") &&
		load(m_uniform2f, "glUniform2f") &&
		load(m_uniform1i, "glUniform1i") &&
		load(m_genBuffers, "glGenBuffers") &&
		load(m_bindBuffer, "glBindBuffer") &&
		load(m_bufferData, "glBufferData") &&
		load(m_bufferSubData, "glBufferSubData") &&
		load(m_deleteBuffers, "glDeleteBuffers") &&
		load(m_enableVertexAttribArray, "glEnableVertexAttribArray") &&
		load(m_disableVertexAttribArray, "glDisableVertexAttribArray") &&
		load(m_vertexAttribPointer, "glVertexAttribPointer") &&
		load(m_activeTexture, "glActiveTexture");
}
void GlDrawListRenderer::releaseProgram() { if (m_program)m_deleteProgram(m_program);m_program = 0; }
bool GlDrawListRenderer::createProgram() {
	auto compile = [&](GLenum t, const char* s) {
		GLuint x = m_createShader(t);
		m_shaderSource(x, 1, &s, nullptr);
		m_compileShader(x);GLint ok = 0;
		m_getShaderiv(x, GL_COMPILE_STATUS, &ok);
		if (!ok) {
			m_deleteShader(x);
			return GLuint(0);
		}
		return x;
		};
	GLuint a = compile(GL_VERTEX_SHADER, vs), b = compile(GL_FRAGMENT_SHADER, fs);
	if (!a || !b) {
		if (a)m_deleteShader(a);
		if (b)m_deleteShader(b);return false;
	}
	m_program = m_createProgram();m_attachShader(m_program, a);m_attachShader(m_program, b);
	const char* names[] = { "p","uv","c0","c1","halfSize","radii","aux","border","prim","flags" };
	for (GLuint i = 0;i < 10;++i)m_bindAttribLocation(m_program, i, names[i]);
	m_linkProgram(m_program);
	m_deleteShader(a);
	m_deleteShader(b);GLint ok = 0;
	m_getProgramiv(m_program, GL_LINK_STATUS, &ok);
	if (!ok) {
		releaseProgram();
		return false;
	}
	m_viewportLoc = m_getUniformLocation(m_program, "viewport");
	m_textureLoc = m_getUniformLocation(m_program, "tex");
	m_hasTextureLoc = m_getUniformLocation(m_program, "hasTex");return true;
}
bool GlDrawListRenderer::init(const RendererDesc& d) {
	if (!loadFunctions() || !createProgram())return false;
	resize(d.width, d.height, d.dpiScale);
	m_genBuffers(1, &m_vertexBuffer);
	m_genBuffers(1, &m_indexBuffer);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	return m_vertexBuffer && m_indexBuffer;
}
void GlDrawListRenderer::shutdown() {
	for (auto& t : m_textures)if (t.alive) {
		glDeleteTextures(1, &t.name);
		t.alive = false;
	};
	if (m_vertexBuffer)m_deleteBuffers(1, &m_vertexBuffer);
	if (m_indexBuffer)m_deleteBuffers(1, &m_indexBuffer);
	m_vertexBuffer = m_indexBuffer = 0;
	releaseProgram();
	m_textures.clear();
}
void GlDrawListRenderer::resize(u32 w, u32 h, f32) {
	m_width = w;m_height = h;glViewport(0, 0, GLsizei(w), GLsizei(h));
}
TextureHandle GlDrawListRenderer::createTexture(const TextureDesc& d) {
	if (!d.width || !d.height)return{};
	GLuint name = 0;glGenTextures(1, &name);
	glBindTexture(GL_TEXTURE_2D, name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, d.linearFilter ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, d.linearFilter ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLint fmt = d.format == TextureFormat::A8 ? GL_ALPHA : GL_RGBA;
	glTexImage2D(GL_TEXTURE_2D, 0, fmt, GLsizei(d.width), GLsizei(d.height), 0, fmt, GL_UNSIGNED_BYTE, d.pixels);
	for (u32 i = 0;i < m_textures.size();++i)if (!m_textures[i].alive) {
		auto& t = m_textures[i];t.name = name;
		t.alive = true;t.format = d.format;
		return{ i,t.generation };
	}m_textures.push_back({ name,1,true,d.format });
	return{ u32(m_textures.size() - 1),1 };
}
GlDrawListRenderer::Texture* GlDrawListRenderer::texture(TextureHandle h) {
	return h.index < m_textures.size() && m_textures[h.index].alive &&
		m_textures[h.index].generation == h.gen ? &m_textures[h.index] : nullptr;
}
void GlDrawListRenderer::updateTexture(TextureHandle h, const void* p, Recti r) {
	auto* t = texture(h);if (!t || !p || r.w <= 0 || r.h <= 0)return;
	glBindTexture(GL_TEXTURE_2D, t->name);
	glTexSubImage2D(GL_TEXTURE_2D, 0, r.x, r.y, r.w, r.h, t->format == TextureFormat::A8 ? GL_ALPHA : GL_RGBA, GL_UNSIGNED_BYTE, p);
}
void GlDrawListRenderer::destroyTexture(TextureHandle h) {
	auto* t = texture(h);if (!t)return;
	glDeleteTextures(1, &t->name);t->alive = false;t->name = 0;++t->generation;
}
void GlDrawListRenderer::beginFrame(const FrameInfo& f) {
	if (f.width != m_width || f.height != m_height)resize(f.width, f.height, f.dpiScale);
	glViewport(0, 0, GLsizei(m_width), GLsizei(m_height));
	glClearColor(f.clear.r, f.clear.g, f.clear.b, f.clear.a);
	glClear(GL_COLOR_BUFFER_BIT);
}
void GlDrawListRenderer::submit(const DrawList& dl) {
	if (!dl.nVerts || !dl.nIdx || !dl.nCmds)return;
	size_t vb = size_t(dl.nVerts) * sizeof(Vertex), ib = size_t(dl.nIdx) * sizeof(u32);
	if (vb > size_t(std::numeric_limits<XuiGLsizeiptr>::max()) || ib > size_t(std::numeric_limits<XuiGLsizeiptr>::max())) return;
	m_useProgram(m_program);
	m_uniform2f(m_viewportLoc, float(m_width), float(m_height));
	m_uniform1i(m_textureLoc, 0);
	m_activeTexture(GL_TEXTURE0);
	m_bindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
	m_bufferData(GL_ARRAY_BUFFER, XuiGLsizeiptr(vb), nullptr, GL_STREAM_DRAW);
	m_bufferSubData(GL_ARRAY_BUFFER, 0, XuiGLsizeiptr(vb), dl.verts);
	m_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
	m_bufferData(GL_ELEMENT_ARRAY_BUFFER, XuiGLsizeiptr(ib), nullptr, GL_STREAM_DRAW);
	m_bufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, XuiGLsizeiptr(ib), dl.idx);
	GLsizei stride = sizeof(Vertex);
	auto at = [&](GLuint i, GLint n, GLenum t, bool norm, size_t o) {
		m_enableVertexAttribArray(i);
		m_vertexAttribPointer(i, n, t, norm ? GL_TRUE : GL_FALSE, stride, reinterpret_cast<const void*>(o));
		};
	at(0, 2, GL_FLOAT, false, offsetof(Vertex, x));
	at(1, 2, GL_FLOAT, false, offsetof(Vertex, u));
	at(2, 4, GL_UNSIGNED_BYTE, true, offsetof(Vertex, c0));
	at(3, 4, GL_UNSIGNED_BYTE, true, offsetof(Vertex, c1));
	at(4, 2, GL_FLOAT, false, offsetof(Vertex, hx));
	at(5, 4, GL_UNSIGNED_SHORT, false, offsetof(Vertex, rad));
	at(6, 2, GL_FLOAT, false, offsetof(Vertex, aux0));
	at(7, 1, GL_UNSIGNED_SHORT, false, offsetof(Vertex, border));
	at(8, 1, GL_UNSIGNED_BYTE, false, offsetof(Vertex, prim));
	at(9, 1, GL_UNSIGNED_BYTE, false, offsetof(Vertex, flags));
	for (u32 i = 0;i < dl.nCmds;++i) {
		const auto& c = dl.cmds[i];
		if (c.kind != DrawCmd::Triangles || !c.idxCount)continue;
		i32 x = std::clamp(c.clip.x, 0, i32(m_width)),
			y = std::clamp(c.clip.y, 0, i32(m_height)),
			r = std::clamp(c.clip.x + c.clip.w, x, i32(m_width)),
			b = std::clamp(c.clip.y + c.clip.h, y, i32(m_height));
		if (r <= x || b <= y)continue;
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, i32(m_height) - b, r - x, b - y);
		auto* t = texture(c.tex);bool has = t != nullptr;
		if (has)glBindTexture(GL_TEXTURE_2D, t->name);
		m_uniform1i(m_hasTextureLoc, has ? 1 : 0);
		glDrawElements(GL_TRIANGLES, GLsizei(c.idxCount),
			GL_UNSIGNED_INT, reinterpret_cast<const void*>(size_t(c.idxOffset) * sizeof(u32)));
	}
	for (GLuint i = 0;i < 10;++i)m_disableVertexAttribArray(i);
	m_bindBuffer(GL_ARRAY_BUFFER, 0);
	m_bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	m_useProgram(0);
	glDisable(GL_SCISSOR_TEST);
}

