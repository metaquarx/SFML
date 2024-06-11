////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2023 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/TextureSaver.hpp>

#include <SFML/Window/Context.hpp>
#include <SFML/Window/Window.hpp>

#include <SFML/System/Err.hpp>

#include <atomic>
#include <ostream>
#include <utility>

#include <cassert>
#include <climits>
#include <cstring>


namespace
{
// A nested named namespace is used here to allow unity builds of SFML.
namespace TextureImpl
{
// Thread-safe unique identifier generator,
// is used for states cache (see RenderTarget)
std::uint64_t getUniqueId() noexcept
{
    static std::atomic<std::uint64_t> id(1); // start at 1, zero is "no texture"

    return id.fetch_add(1);
}
} // namespace TextureImpl
} // namespace


namespace sf
{
////////////////////////////////////////////////////////////
Texture::Texture() : m_cacheId(TextureImpl::getUniqueId())
{
}


////////////////////////////////////////////////////////////
Texture::Texture(const Texture& copy) :
GlResource(copy),
m_isSmooth(copy.m_isSmooth),
m_sRgb(copy.m_sRgb),
m_isRepeated(copy.m_isRepeated),
m_cacheId(TextureImpl::getUniqueId())
{
    if (copy.m_texture)
    {
        if (create(copy.getSize()))
        {
            update(copy);
        }
        else
        {
            err() << "Failed to copy texture, failed to create new texture" << std::endl;
        }
    }
}


////////////////////////////////////////////////////////////
Texture::~Texture()
{
    // Destroy the OpenGL texture
    if (m_texture)
    {
        const TransientContextLock lock;

        const GLuint texture = m_texture;
        glCheck(glDeleteTextures(1, &texture));
    }
}

////////////////////////////////////////////////////////////
Texture::Texture(Texture&& right) noexcept :
m_size(std::exchange(right.m_size, {})),
m_texture(std::exchange(right.m_texture, 0)),
m_isSmooth(std::exchange(right.m_isSmooth, false)),
m_sRgb(std::exchange(right.m_sRgb, false)),
m_isRepeated(std::exchange(right.m_isRepeated, false)),
m_fboAttachment(std::exchange(right.m_fboAttachment, false)),
m_cacheId(std::exchange(right.m_cacheId, 0))
{
}

////////////////////////////////////////////////////////////
Texture& Texture::operator=(Texture&& right) noexcept
{
    // Catch self-moving.
    if (&right == this)
    {
        return *this;
    }

    // Destroy the OpenGL texture
    if (m_texture)
    {
        const TransientContextLock lock;

        const GLuint texture = m_texture;
        glCheck(glDeleteTextures(1, &texture));
    }

    // Move old to new.
    m_size          = std::exchange(right.m_size, {});
    m_texture       = std::exchange(right.m_texture, 0);
    m_isSmooth      = std::exchange(right.m_isSmooth, false);
    m_sRgb          = std::exchange(right.m_sRgb, false);
    m_isRepeated    = std::exchange(right.m_isRepeated, false);
    m_fboAttachment = std::exchange(right.m_fboAttachment, false);
    m_cacheId       = std::exchange(right.m_cacheId, 0);
    return *this;
}


////////////////////////////////////////////////////////////
bool Texture::create(const Vector2u& size)
{
    // Check if texture parameters are valid before creating it
    if ((size.x == 0) || (size.y == 0))
    {
        err() << "Failed to create texture, invalid size (" << size.x << "x" << size.y << ")" << std::endl;
        return false;
    }

    const TransientContextLock lock;

    // Check the maximum texture size
    const unsigned int maxSize = getMaximumSize();
    if ((size.x > maxSize) || (size.y > maxSize))
    {
        err() << "Failed to create texture, its internal size is too high "
              << "(" << size.x << "x" << size.y << ", "
              << "maximum is " << maxSize << "x" << maxSize << ")" << std::endl;
        return false;
    }

    // All the validity checks passed, we can store the new texture settings
    m_size          = size;
    m_fboAttachment = false;

    // Create the OpenGL texture if it doesn't exist yet
    if (!m_texture)
    {
        GLuint texture;
        glCheck(glGenTextures(1, &texture));
        m_texture = texture;
    }

    // Make sure that the current texture binding will be preserved
    const priv::TextureSaver save;

    // Initialize the texture
    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glTexImage2D(GL_TEXTURE_2D,
                         0,
                         (m_sRgb ? GL_SRGB8_ALPHA8 : GL_RGBA),
                         static_cast<GLsizei>(m_size.x),
                         static_cast<GLsizei>(m_size.y),
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         nullptr));
    glCheck(glTexParameteri(GL_TEXTURE_2D,
                            GL_TEXTURE_WRAP_S,
                            m_isRepeated ? GL_REPEAT : GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_2D,
                            GL_TEXTURE_WRAP_T,
                            m_isRepeated ? GL_REPEAT : GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));
    m_cacheId = TextureImpl::getUniqueId();

    m_hasMipmap = false;

    return true;
}


////////////////////////////////////////////////////////////
bool Texture::loadFromFile(const std::filesystem::path& filename, const IntRect& area)
{
    Image image;
    return image.loadFromFile(filename) && loadFromImage(image, area);
}


////////////////////////////////////////////////////////////
bool Texture::loadFromMemory(const void* data, std::size_t size, const IntRect& area)
{
    Image image;
    return image.loadFromMemory(data, size) && loadFromImage(image, area);
}


////////////////////////////////////////////////////////////
bool Texture::loadFromStream(InputStream& stream, const IntRect& area)
{
    Image image;
    return image.loadFromStream(stream) && loadFromImage(image, area);
}


////////////////////////////////////////////////////////////
bool Texture::loadFromImage(const Image& image, const IntRect& area)
{
    // Retrieve the image size
    const auto [width, height] = Vector2i(image.getSize());

    // Load the entire image if the source area is either empty or contains the whole image
    if (area.width == 0 || (area.height == 0) ||
        ((area.left <= 0) && (area.top <= 0) && (area.width >= width) && (area.height >= height)))
    {
        // Load the entire image
        if (create(image.getSize()))
        {
            update(image);

            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        // Load a sub-area of the image

        // Adjust the rectangle to the size of the image
        IntRect rectangle = area;
        if (rectangle.left < 0)
            rectangle.left = 0;
        if (rectangle.top < 0)
            rectangle.top = 0;
        if (rectangle.left + rectangle.width > width)
            rectangle.width = width - rectangle.left;
        if (rectangle.top + rectangle.height > height)
            rectangle.height = height - rectangle.top;

        // Create the texture and upload the pixels
        if (create(Vector2u(rectangle.getSize())))
        {
            const TransientContextLock lock;

            // Make sure that the current texture binding will be preserved
            const priv::TextureSaver save;

            // Copy the pixels to the texture, row by row
            const std::uint8_t* pixels = image.getPixelsPtr() + 4 * (rectangle.left + (width * rectangle.top));
            glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
            for (int i = 0; i < rectangle.height; ++i)
            {
                glCheck(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, i, rectangle.width, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
                pixels += 4 * width;
            }

            glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));
            m_hasMipmap = false;

            // Force an OpenGL flush, so that the texture will appear updated
            // in all contexts immediately (solves problems in multi-threaded apps)
            glCheck(glFlush());

            return true;
        }
        else
        {
            return false;
        }
    }
}


////////////////////////////////////////////////////////////
Vector2u Texture::getSize() const
{
    return m_size;
}


////////////////////////////////////////////////////////////
Image Texture::copyToImage() const
{
    // Easy case: empty texture
    if (!m_texture)
        return Image();

    const TransientContextLock lock;

    // Make sure that the current texture binding will be preserved
    const priv::TextureSaver save;

    // Create an array of pixels
    std::vector<std::uint8_t> pixels(m_size.x * m_size.y * 4);

#ifdef SFML_OPENGL_ES

    // OpenGL ES doesn't have the glGetTexImage function, the only way to read
    // from a texture is to bind it to a FBO and use glReadPixels
    GLuint frameBuffer = 0;
    glCheck(glGenFramebuffers(1, &frameBuffer));
    if (frameBuffer)
    {
        GLint previousFrameBuffer;
        glCheck(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFrameBuffer));

        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer));
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0));
        glCheck(glReadPixels(0, 0, m_size.x, m_size.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data()));
        glCheck(glDeleteFramebuffers(1, &frameBuffer));

        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, previousFrameBuffer));
    }

#else

    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data()));

#endif // SFML_OPENGL_ES

    // Create the image
    Image image;
    image.create(m_size, pixels.data());

    return image;
}


////////////////////////////////////////////////////////////
void Texture::update(const std::uint8_t* pixels)
{
    // Update the whole texture
    update(pixels, m_size, {0, 0});
}


////////////////////////////////////////////////////////////
void Texture::update(const std::uint8_t* pixels, const Vector2u& size, const Vector2u& dest)
{
    assert(dest.x + size.x <= m_size.x);
    assert(dest.y + size.y <= m_size.y);

    if (pixels && m_texture)
    {
        const TransientContextLock lock;

        // Make sure that the current texture binding will be preserved
        const priv::TextureSaver save;

        // Copy pixels from the given array to the texture
        glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
        glCheck(glTexSubImage2D(GL_TEXTURE_2D,
                                0,
                                static_cast<GLint>(dest.x),
                                static_cast<GLint>(dest.y),
                                static_cast<GLsizei>(size.x),
                                static_cast<GLsizei>(size.y),
                                GL_RGBA,
                                GL_UNSIGNED_BYTE,
                                pixels));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));
        m_hasMipmap     = false;
        m_cacheId       = TextureImpl::getUniqueId();

        // Force an OpenGL flush, so that the texture data will appear updated
        // in all contexts immediately (solves problems in multi-threaded apps)
        glCheck(glFlush());
    }
}


////////////////////////////////////////////////////////////
void Texture::update(const Texture& texture)
{
    // Update the whole texture
    update(texture, {0, 0});
}


////////////////////////////////////////////////////////////
void Texture::update(const Texture& texture, const Vector2u& dest)
{
    assert(dest.x + texture.m_size.x <= m_size.x);
    assert(dest.y + texture.m_size.y <= m_size.y);

    if (!m_texture || !texture.m_texture)
        return;

#ifndef SFML_OPENGL_ES

    const TransientContextLock lock;

    // Save the current bindings so we can restore them after we are done
    GLint readFramebuffer = 0;
    GLint drawFramebuffer = 0;

    glCheck(glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer));
    glCheck(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer));

    // Create the framebuffers
    GLuint sourceFrameBuffer = 0;
    GLuint destFrameBuffer   = 0;
    glCheck(glGenFramebuffers(1, &sourceFrameBuffer));
    glCheck(glGenFramebuffers(1, &destFrameBuffer));

    if (!sourceFrameBuffer || !destFrameBuffer)
    {
        err() << "Cannot copy texture, failed to create a frame buffer object" << std::endl;
        return;
    }

    // Link the source texture to the source frame buffer
    glCheck(glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFrameBuffer));
    glCheck(glFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                                         GL_COLOR_ATTACHMENT0,
                                         GL_TEXTURE_2D,
                                         texture.m_texture,
                                         0));

    // Link the destination texture to the destination frame buffer
    glCheck(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFrameBuffer));
    glCheck(
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0));

    // A final check, just to be sure...
    GLenum sourceStatus;
    glCheck(sourceStatus = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER));

    GLenum destStatus;
    glCheck(destStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));

    if ((sourceStatus == GL_FRAMEBUFFER_COMPLETE) && (destStatus == GL_FRAMEBUFFER_COMPLETE))
    {
        // Blit the texture contents from the source to the destination texture
        glCheck(glBlitFramebuffer(0,
                                        0,
                                        static_cast<GLint>(texture.m_size.x),
                                        static_cast<GLint>(texture.m_size.y), // Source rectangle
                                        static_cast<GLint>(dest.x),
                                        static_cast<GLint>(dest.y),
                                        static_cast<GLint>(dest.x + texture.m_size.x),
                                        static_cast<GLint>(dest.y + texture.m_size.y), // Destination rectangle
                                        GL_COLOR_BUFFER_BIT,
                                        GL_NEAREST));
    }
    else
    {
        err() << "Cannot copy texture, failed to link texture to frame buffer" << std::endl;
    }

    // Restore previously bound framebuffers
    glCheck(glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFramebuffer)));
    glCheck(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFramebuffer)));

    // Delete the framebuffers
    glCheck(glDeleteFramebuffers(1, &sourceFrameBuffer));
    glCheck(glDeleteFramebuffers(1, &destFrameBuffer));

    // Make sure that the current texture binding will be preserved
    const priv::TextureSaver save;

    // Set the parameters of this texture
    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));
    m_hasMipmap     = false;
    m_cacheId       = TextureImpl::getUniqueId();

    // Force an OpenGL flush, so that the texture data will appear updated
    // in all contexts immediately (solves problems in multi-threaded apps)
    glCheck(glFlush());

    return;

#endif // SFML_OPENGL_ES

    update(texture.copyToImage(), dest);
}


////////////////////////////////////////////////////////////
void Texture::update(const Image& image)
{
    // Update the whole texture
    update(image.getPixelsPtr(), image.getSize(), {0, 0});
}


////////////////////////////////////////////////////////////
void Texture::update(const Image& image, const Vector2u& dest)
{
    update(image.getPixelsPtr(), image.getSize(), dest);
}


////////////////////////////////////////////////////////////
void Texture::update(const Window& window)
{
    update(window, {0, 0});
}


////////////////////////////////////////////////////////////
void Texture::update(const Window& window, const Vector2u& dest)
{
    assert(dest.x + window.getSize().x <= m_size.x);
    assert(dest.y + window.getSize().y <= m_size.y);

    if (m_texture && window.setActive(true))
    {
        const TransientContextLock lock;

        // Make sure that the current texture binding will be preserved
        const priv::TextureSaver save;

        // Copy pixels from the back-buffer to the texture
        glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
        glCheck(glCopyTexSubImage2D(GL_TEXTURE_2D,
                                    0,
                                    static_cast<GLint>(dest.x),
                                    static_cast<GLint>(dest.y),
                                    0,
                                    0,
                                    static_cast<GLsizei>(window.getSize().x),
                                    static_cast<GLsizei>(window.getSize().y)));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));
        m_hasMipmap     = false;
        m_cacheId       = TextureImpl::getUniqueId();

        // Force an OpenGL flush, so that the texture will appear updated
        // in all contexts immediately (solves problems in multi-threaded apps)
        glCheck(glFlush());
    }
}


////////////////////////////////////////////////////////////
void Texture::setSmooth(bool smooth)
{
    if (smooth != m_isSmooth)
    {
        m_isSmooth = smooth;

        if (m_texture)
        {
            const TransientContextLock lock;

            // Make sure that the current texture binding will be preserved
            const priv::TextureSaver save;

            glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
            glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));

            if (m_hasMipmap)
            {
                glCheck(glTexParameteri(GL_TEXTURE_2D,
                                        GL_TEXTURE_MIN_FILTER,
                                        m_isSmooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR));
            }
            else
            {
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));
            }
        }
    }
}


////////////////////////////////////////////////////////////
bool Texture::isSmooth() const
{
    return m_isSmooth;
}


////////////////////////////////////////////////////////////
void Texture::setSrgb(bool sRgb)
{
    m_sRgb = sRgb;
}


////////////////////////////////////////////////////////////
bool Texture::isSrgb() const
{
    return m_sRgb;
}


////////////////////////////////////////////////////////////
void Texture::setRepeated(bool repeated)
{
    if (repeated != m_isRepeated)
    {
        m_isRepeated = repeated;

        if (m_texture)
        {
            const TransientContextLock lock;

            // Make sure that the current texture binding will be preserved
            const priv::TextureSaver save;

            glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
            glCheck(
                glTexParameteri(GL_TEXTURE_2D,
                                GL_TEXTURE_WRAP_S,
                                m_isRepeated ? GL_REPEAT : GL_CLAMP_TO_EDGE));
            glCheck(
                glTexParameteri(GL_TEXTURE_2D,
                                GL_TEXTURE_WRAP_T,
                                m_isRepeated ? GL_REPEAT : GL_CLAMP_TO_EDGE));
        }
    }
}


////////////////////////////////////////////////////////////
bool Texture::isRepeated() const
{
    return m_isRepeated;
}


////////////////////////////////////////////////////////////
bool Texture::generateMipmap()
{
    if (!m_texture)
        return false;

    const TransientContextLock lock;

    // Make sure that the current texture binding will be preserved
    const priv::TextureSaver save;

    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glGenerateMipmap(GL_TEXTURE_2D));
    glCheck(glTexParameteri(GL_TEXTURE_2D,
                            GL_TEXTURE_MIN_FILTER,
                            m_isSmooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR));

    m_hasMipmap = true;

    return true;
}


////////////////////////////////////////////////////////////
void Texture::invalidateMipmap()
{
    if (!m_hasMipmap)
        return;

    const TransientContextLock lock;

    // Make sure that the current texture binding will be preserved
    const priv::TextureSaver save;

    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));

    m_hasMipmap = false;
}


////////////////////////////////////////////////////////////
void Texture::bind(const Texture* texture)
{
    const TransientContextLock lock;

    if (texture && texture->m_texture)
    {
        // Bind the texture
        glCheck(glBindTexture(GL_TEXTURE_2D, texture->m_texture));
    }
    else
    {
        // Bind no texture
        glCheck(glBindTexture(GL_TEXTURE_2D, 0));
    }
}


////////////////////////////////////////////////////////////
unsigned int Texture::getMaximumSize()
{
    static const unsigned int size = []()
    {
        const TransientContextLock transientLock;

        GLint value = 0;

        glCheck(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value));

        return static_cast<unsigned int>(value);
    }();

    return size;
}


////////////////////////////////////////////////////////////
Texture& Texture::operator=(const Texture& right)
{
    Texture temp(right);

    swap(temp);

    return *this;
}


////////////////////////////////////////////////////////////
void Texture::swap(Texture& right) noexcept
{
    std::swap(m_size, right.m_size);
    std::swap(m_texture, right.m_texture);
    std::swap(m_isSmooth, right.m_isSmooth);
    std::swap(m_sRgb, right.m_sRgb);
    std::swap(m_isRepeated, right.m_isRepeated);
    std::swap(m_fboAttachment, right.m_fboAttachment);
    std::swap(m_hasMipmap, right.m_hasMipmap);

    m_cacheId       = TextureImpl::getUniqueId();
    right.m_cacheId = TextureImpl::getUniqueId();
}


////////////////////////////////////////////////////////////
unsigned int Texture::getNativeHandle() const
{
    return m_texture;
}


////////////////////////////////////////////////////////////
void swap(Texture& left, Texture& right) noexcept
{
    left.swap(right);
}

} // namespace sf
