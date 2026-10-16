#pragma once

#include <xui/render/renderer.hpp>
#include <GLFW/glfw3.h>
#include <cstddef>
#include <vector>

using XuiGLsizeiptr = std::ptrdiff_t;
using XuiGLintptr = std::ptrdiff_t;
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE0 0x84C0
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#endif

/// OpenGL 2.1 renderer for the backend-neutral Xui DrawList contract.
/// GLFW supplies only the context and procedure loader; widget/layout logic stays in xui_widgets.
class GlDrawListRenderer final : public xui::Renderer {
public:
	bool init(const xui::RendererDesc&) override;
	void shutdown() override;
	void resize(xui::u32, xui::u32, xui::f32) override;
	xui::TextureHandle createTexture(const xui::TextureDesc&) override;
	void updateTexture(xui::TextureHandle, const void*, xui::Recti) override;
	void destroyTexture(xui::TextureHandle) override;
	void beginFrame(const xui::FrameInfo&) override;
	void submit(const xui::DrawList&) override;
	void endFrame() override {}
	void present() override {}
	xui::RendererCaps caps() const override { return { "OpenGL 2.1 DrawList renderer", 4096, false, false, false }; }
	xui::u32 width() const { return m_width; }
	xui::u32 height() const { return m_height; }

private:
	struct Texture { GLuint name = 0; xui::u32 generation = 1; bool alive = false; xui::TextureFormat format = xui::TextureFormat::RGBA8; };
	Texture* texture(xui::TextureHandle);
	bool loadFunctions();
	bool createProgram();
	void releaseProgram();

	using CreateShaderFn = GLuint(APIENTRY*)(GLenum);
	using ShaderSourceFn = void (APIENTRY*)(GLuint, GLsizei, const char* const*, const GLint*);
	using CompileShaderFn = void (APIENTRY*)(GLuint);
	using GetShaderivFn = void (APIENTRY*)(GLuint, GLenum, GLint*);
	using DeleteShaderFn = void (APIENTRY*)(GLuint);
	using CreateProgramFn = GLuint(APIENTRY*)();
	using AttachShaderFn = void (APIENTRY*)(GLuint, GLuint);
	using BindAttribLocationFn = void (APIENTRY*)(GLuint, GLuint, const char*);
	using LinkProgramFn = void (APIENTRY*)(GLuint);
	using GetProgramivFn = void (APIENTRY*)(GLuint, GLenum, GLint*);
	using DeleteProgramFn = void (APIENTRY*)(GLuint);
	using UseProgramFn = void (APIENTRY*)(GLuint);
	using GetUniformLocationFn = GLint(APIENTRY*)(GLuint, const char*);
	using Uniform2fFn = void (APIENTRY*)(GLint, GLfloat, GLfloat);
	using Uniform1iFn = void (APIENTRY*)(GLint, GLint);
	using GenBuffersFn = void (APIENTRY*)(GLsizei, GLuint*);
	using BindBufferFn = void (APIENTRY*)(GLenum, GLuint);
	using BufferDataFn = void (APIENTRY*)(GLenum, XuiGLsizeiptr, const void*, GLenum);
	using BufferSubDataFn = void (APIENTRY*)(GLenum, XuiGLintptr, XuiGLsizeiptr, const void*);
	using DeleteBuffersFn = void (APIENTRY*)(GLsizei, const GLuint*);
	using EnableVertexAttribArrayFn = void (APIENTRY*)(GLuint);
	using DisableVertexAttribArrayFn = void (APIENTRY*)(GLuint);
	using VertexAttribPointerFn = void (APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
	using ActiveTextureFn = void (APIENTRY*)(GLenum);

	CreateShaderFn m_createShader = nullptr; ShaderSourceFn m_shaderSource = nullptr;
	CompileShaderFn m_compileShader = nullptr; GetShaderivFn m_getShaderiv = nullptr; DeleteShaderFn m_deleteShader = nullptr;
	CreateProgramFn m_createProgram = nullptr; AttachShaderFn m_attachShader = nullptr; BindAttribLocationFn m_bindAttribLocation = nullptr;
	LinkProgramFn m_linkProgram = nullptr; GetProgramivFn m_getProgramiv = nullptr; DeleteProgramFn m_deleteProgram = nullptr;
	UseProgramFn m_useProgram = nullptr; GetUniformLocationFn m_getUniformLocation = nullptr;
	Uniform2fFn m_uniform2f = nullptr; Uniform1iFn m_uniform1i = nullptr;
	GenBuffersFn m_genBuffers = nullptr; BindBufferFn m_bindBuffer = nullptr; BufferDataFn m_bufferData = nullptr;
	BufferSubDataFn m_bufferSubData = nullptr; DeleteBuffersFn m_deleteBuffers = nullptr;
	EnableVertexAttribArrayFn m_enableVertexAttribArray = nullptr; DisableVertexAttribArrayFn m_disableVertexAttribArray = nullptr;
	VertexAttribPointerFn m_vertexAttribPointer = nullptr; ActiveTextureFn m_activeTexture = nullptr;

	GLuint m_program = 0, m_vertexBuffer = 0, m_indexBuffer = 0;
	GLint m_viewportLoc = -1, m_textureLoc = -1, m_hasTextureLoc = -1;
	xui::u32 m_width = 0, m_height = 0;
	std::vector<Texture> m_textures;
};
