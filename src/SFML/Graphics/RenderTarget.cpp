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
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/VertexBuffer.hpp>

#include <SFML/Window/Context.hpp>

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <iostream>
#include <mutex>
#include <ostream>
#include <unordered_map>

#include <cassert>
#include <cmath>
#include <cstddef>


namespace
{
// A nested named namespace is used here to allow unity builds of SFML.
namespace RenderTargetImpl
{
// Mutex to protect ID generation and our context-RenderTarget-map
std::recursive_mutex& getMutex()
{
    static std::recursive_mutex mutex;
    return mutex;
}

// Unique identifier, used for identifying RenderTargets when
// tracking the currently active RenderTarget within a given context
std::uint64_t getUniqueId()
{
    const std::lock_guard lock(getMutex());
    static std::uint64_t  id = 1; // start at 1, zero is "no RenderTarget"
    return id++;
}

// Map to help us detect whether a different RenderTarget
// has been activated within a single context
using ContextRenderTargetMap = std::unordered_map<std::uint64_t, std::uint64_t>;
ContextRenderTargetMap& getContextRenderTargetMap()
{
    static ContextRenderTargetMap contextRenderTargetMap;
    return contextRenderTargetMap;
}

// Check if a RenderTarget with the given ID is active in the current context
bool isActive(std::uint64_t id)
{
    const auto it = getContextRenderTargetMap().find(sf::Context::getActiveContextId());
    return (it != getContextRenderTargetMap().end()) && (it->second == id);
}

// Convert an sf::BlendMode::Factor constant to the corresponding OpenGL constant.
std::uint32_t factorToGlConstant(sf::BlendMode::Factor blendFactor)
{
    // clang-format off
    switch (blendFactor)
    {
        case sf::BlendMode::Zero:             return GL_ZERO;
        case sf::BlendMode::One:              return GL_ONE;
        case sf::BlendMode::SrcColor:         return GL_SRC_COLOR;
        case sf::BlendMode::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
        case sf::BlendMode::DstColor:         return GL_DST_COLOR;
        case sf::BlendMode::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
        case sf::BlendMode::SrcAlpha:         return GL_SRC_ALPHA;
        case sf::BlendMode::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        case sf::BlendMode::DstAlpha:         return GL_DST_ALPHA;
        case sf::BlendMode::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
    }
    // clang-format on

    sf::err() << "Invalid value for sf::BlendMode::Factor! Fallback to sf::BlendMode::Zero." << std::endl;
    assert(false);
    return GL_ZERO;
}


// Convert an sf::BlendMode::BlendEquation constant to the corresponding OpenGL constant.
std::uint32_t equationToGlConstant(sf::BlendMode::Equation blendEquation)
{
    switch (blendEquation)
    {
        case sf::BlendMode::Add:
            return GL_FUNC_ADD;
        case sf::BlendMode::Subtract:
            return GL_FUNC_SUBTRACT;
        case sf::BlendMode::ReverseSubtract:
            return GL_FUNC_REVERSE_SUBTRACT;
        case sf::BlendMode::Min:
            return GL_MIN;
        case sf::BlendMode::Max:
            return GL_MAX;
    }
}


// Minimum number of vertices for each primitive type
std::size_t minVertexCount(sf::PrimitiveType type) {
    switch (type) {
        case Points:        return 1
        case Lines:         [[fallthrough]];
        case LineStrip:     return 2;
        case Triangles:     [[fallthrough]];
        case TriangleStrip: [[fallthrough]];
        case TriangleFan:   return 3;
    }
}
} // namespace RenderTargetImpl
} // namespace


namespace sf
{

////////////////////////////////////////////////////////////
RenderTarget::~RenderTarget() {
}

////////////////////////////////////////////////////////////
void RenderTarget::clear(const Color& color)
{
    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        // Unbind texture to fix RenderTexture preventing clear
        Texture::bind(nullptr);

        glCheck(glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
        glCheck(glClear(GL_COLOR_BUFFER_BIT));
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::setView(const View& view)
{
    m_view = view;
}


////////////////////////////////////////////////////////////
const View& RenderTarget::getView() const
{
    return m_view;
}


////////////////////////////////////////////////////////////
const View& RenderTarget::getDefaultView() const
{
    return m_defaultView;
}


////////////////////////////////////////////////////////////
IntRect RenderTarget::getViewport(const View& view) const
{
    const auto [width, height] = Vector2f(getSize());
    const FloatRect& viewport  = view.getViewport();

    return IntRect(Rect<long>({std::lround(width * viewport.left), std::lround(height * viewport.top)},
                              {std::lround(width * viewport.width), std::lround(height * viewport.height)}));
}


////////////////////////////////////////////////////////////
Vector2f RenderTarget::mapPixelToCoords(const Vector2i& point) const
{
    return mapPixelToCoords(point, getView());
}


////////////////////////////////////////////////////////////
Vector2f RenderTarget::mapPixelToCoords(const Vector2i& point, const View& view) const
{
    // First, convert from viewport coordinates to homogeneous coordinates
    Vector2f        normalized;
    const FloatRect viewport = FloatRect(getViewport(view));
    normalized.x             = -1.f + 2.f * (static_cast<float>(point.x) - viewport.left) / viewport.width;
    normalized.y             = 1.f - 2.f * (static_cast<float>(point.y) - viewport.top) / viewport.height;

    // Then transform by the inverse of the view matrix
    return view.getInverseTransform().transformPoint(normalized);
}


////////////////////////////////////////////////////////////
Vector2i RenderTarget::mapCoordsToPixel(const Vector2f& point) const
{
    return mapCoordsToPixel(point, getView());
}


////////////////////////////////////////////////////////////
Vector2i RenderTarget::mapCoordsToPixel(const Vector2f& point, const View& view) const
{
    // First, transform the point by the view matrix
    const Vector2f normalized = view.getTransform().transformPoint(point);

    // Then convert to viewport coordinates
    Vector2i        pixel;
    const FloatRect viewport = FloatRect(getViewport(view));
    pixel.x                  = static_cast<int>((normalized.x + 1.f) / 2.f * viewport.width + viewport.left);
    pixel.y                  = static_cast<int>((-normalized.y + 1.f) / 2.f * viewport.height + viewport.top);

    return pixel;
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const Drawable& drawable, const RenderStates& states)
{
    drawable.draw(*this, states);
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const Vertex* vertices, std::size_t vertexCount, PrimitiveType type, const RenderStates& states)
{
    // Nothing to draw?
    if (!vertices || vertexCount < RenderTargetImpl::minVertexCount(type))
        return;

    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        auto new_type = type;
        if (type == LineStrip) {
            new_type = Lines;
        } else if (type == TriangleStrip || type == TriangleFan) {
            new_type == Triangles;
        }

        StepState new_state{new_type, states};
        if (m_currentStep.state != new_state) {
            clearOngoingStep();
            m_currentStep.state = new_state;
        }

        auto& verts = m_currentStep.vertices;
        auto& elements = m_currentStep.elements;

        verts.reserve(verts.size() + vertexcount * (2 + 4 + 2)) // xy rgba st
        elements.reserve(elements.size() + (type ==     LineStrip ? (vertexCount - 1) * 2 :
                                            type == TriangleStrip ? (vertexCount - 2) * 3 :
                                            type ==   TriangleFan ? (vertexCount - 2) * 3 :
                                            vertexCount));


        if (type == Points || type == Lines || type == Triangles) {
            for (std::size_t i = 0; i < vertexCount; i++) {
                elements.emplace_back(verts.size() / (2 + 4 + 2));

                auto position = states.transform * vertices[i].position;
                verts.emplace_back(position.x);
                verts.emplace_back(position.y);

                verts.emplace_back(vertices[i].color.r / 255.f);
                verts.emplace_back(vertices[i].color.g / 255.f);
                verts.emplace_back(vertices[i].color.b / 255.f);
                verts.emplace_back(vertices[i].color.a / 255.f);

                verts.emplace_back(vertices[i].texCoords.x);
                verts.emplace_back(vertices[i].texCoords.y);
            }
        } else if (type == LineStrip) {
            for (std::size_t i = 0; i < vertexCount; i++) {
                if (i > 0) {
                    elements.emplace_back(verts.size() / (2 + 4 + 2) - 1);
                    elements.emplace_back(verts.size() / (2 + 4 + 2));
                }

                auto position = states.transform * vertices[i].position;
                verts.emplace_back(position.x);
                verts.emplace_back(position.y);

                verts.emplace_back(vertices[i].color.r / 255.f);
                verts.emplace_back(vertices[i].color.g / 255.f);
                verts.emplace_back(vertices[i].color.b / 255.f);
                verts.emplace_back(vertices[i].color.a / 255.f);

                verts.emplace_back(vertices[i].texCoords.x);
                verts.emplace_back(vertices[i].texCoords.y);
            }
        } else if (type == TriangleStrip) {
            for (std::size_t i = 0; i < vertexCount; i++)
        } else if (type == TriangleFan) {
            auto root = verts.size() / (2 + 4 + 2);

            for (std::size_t i = 0; i < vertexCount; i++) {
                if (i > 2) {
                    elements.emplace_back(root);
                    elements.emplace_back(verts.size() / (2 + 4 + 2) - 1);
                    elements.emplace_back(verts.size() / (2 + 4 + 2));
                }

                auto position = states.transform * vertices[i].position;
                verts.emplace_back(position.x);
                verts.emplace_back(position.y);

                verts.emplace_back(vertices[i].color.r / 255.f);
                verts.emplace_back(vertices[i].color.g / 255.f);
                verts.emplace_back(vertices[i].color.b / 255.f);
                verts.emplace_back(vertices[i].color.a / 255.f);

                verts.emplace_back(vertices[i].texCoords.x);
                verts.emplace_back(vertices[i].texCoords.y);

            }
        }
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const VertexBuffer& vertexBuffer, const RenderStates& states)
{
    draw(vertexBuffer, 0, vertexBuffer.getVertexCount(), states);
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const VertexBuffer& vertexBuffer, std::size_t firstVertex, std::size_t vertexCount, const RenderStates& states)
{
    // Sanity check
    if (firstVertex > vertexBuffer.getVertexCount())
        return;

    // Clamp vertexCount to something that makes sense
    vertexCount = std::min(vertexCount, vertexBuffer.getVertexCount() - firstVertex);

    // Nothing to draw?
    if (!vertexCount || !vertexBuffer.getNativeHandle())
        return;

    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        clearOngoingStep();
        m_currentStep.state = {vertexBuffer.getPrimitiveType(), states};
        m_currentStep.vbo = vertexBuffer.getNativeHandle();
        m_currentStep.overruled = true;
        m_currentStep.vertices.emplace_back(firstVertex);
        m_currentStep.vertices.emplace_back(vertexCount);
        m_steps.emplace_back(std::move(m_currentStep));

        m_stepsIdx++;
        m_currentStep = DrawStep{};
    }
}


////////////////////////////////////////////////////////////
bool RenderTarget::isSrgb() const
{
    // By default sRGB encoding is not enabled for an arbitrary RenderTarget
    return false;
}


////////////////////////////////////////////////////////////
bool RenderTarget::setActive(bool active)
{
    // Mark this RenderTarget as active or no longer active in the tracking map
    {
        const std::lock_guard lock(RenderTargetImpl::getMutex());

        const std::uint64_t contextId = Context::getActiveContextId();

        using RenderTargetImpl::getContextRenderTargetMap;
        auto& contextRenderTargetMap = getContextRenderTargetMap();
        auto  it                     = contextRenderTargetMap.find(contextId);

        if (active)
        {
            if (it == contextRenderTargetMap.end())
            {
                contextRenderTargetMap[contextId] = m_id;
            }
            else if (it->second != m_id)
            {
                it->second = m_id;
            }
        }
        else
        {
            if (it != contextRenderTargetMap.end())
                contextRenderTargetMap.erase(it);
        }
    }

    return true;
}


////////////////////////////////////////////////////////////
void RenderTarget::setDefaultShader(Shader* shader) {
    if (shader) {
        m_defaultShader = shader;
    } else {
        m_defaultShader = m_fallbackShader.get();
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::flush()
{
    clearOngoingStep();
    m_stepsIdx = 0;

    if (RenderTargetImpl::isActive(m_id) || setActive(true)) {
        if (isSrgb()) {
            glCheck(glEnable(GL_FRAMEBUFFER_SRGB));
        } else {
            glCheck(glDisable(GL_FRAMEBUFFER_SRGB));
        }

        applyBlendMode(BlendAlpha);

        const IntRect viewport = getViewport(m_view);
        const int     top      = static_cast<int>(getSize().y) - (viewport.top + viewport.height);
        glCheck(glViewport(viewport.left, top, viewport.width, viewport.height));
        std::cout << "set viewport position (" << viewport.left << "," << top << "), size (" << viewport.width << "," << viewport.height << ")" << std::endl;

        Shader* currentShader = m_defaultShader;
        Shader::bind(currentShader);
        currentShader->setUniform("viewport", Glsl::Mat4(m_view.getTransform().getMatrix()));
        for (const auto& step : m_steps) {

            glCheck(glBindVertexArray(step.vao));

            static constexpr GLenum modes[] = {GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN};
            glCheck(glDrawArrays(modes[static_cast<std::size_t>(step.state.type)], 0, step.vertices.size() / 8));

            std::cout << "draw " << step.vbo << std::endl;
        }

        Shader::bind(nullptr);
    }
}


////////////////////////////////////////////////////////////
RenderTarget::RenderTarget()
: m_stepsIdx{0} {
}

////////////////////////////////////////////////////////////
void RenderTarget::initialize()
{
    // Setup the default and current views
    m_defaultView.reset(FloatRect({0, 0}, Vector2f(getSize())));
    m_view = m_defaultView;

    // Generate a unique ID for this RenderTarget to track
    // whether it is active within a specific context
    m_id = RenderTargetImpl::getUniqueId();

    // Initialise OpenGL state
    m_fallbackShader = std::make_unique<Shader>();
    if (!m_fallbackShader->loadFromMemory("#version 410 core\n"
                                          "layout (location = 0) in vec2 vPosition;\n"
                                          "layout (location = 1) in vec4 vColor;\n"
                                          "layout (location = 2) in vec2 vTexCoords;\n"
                                          "\n"
                                          "out vec2 fTexCoords;\n"
                                          "out vec4 fColor;\n"
                                          "\n"
                                          "uniform mat4 viewport;\n"
                                          "\n"
                                          "void main()\n"
                                          "{\n"
                                          "    gl_Position = viewport * vec4(vPosition, 0.0, 0.0);\n"
                                          "    fTexCoords = vTexCoords;"
                                          "}\n",
                                          "#version 410 core\n"
                                          "in vec2 fTexCoords;\n"
                                          "in vec4 fColor;\n"
                                          "\n"
                                          "out vec4 fragColor;\n"
                                          "\n"
                                          "void main()\n"
                                          "{\n"
                                          "  fragColor = fColor;\n"
                                          "}\n")) {
        sf::err() << "Failed to compile default shaders" << std::endl;
    }
    // TODO texture, colour
    setDefaultShader();
}


////////////////////////////////////////////////////////////
void RenderTarget::applyBlendMode(const BlendMode& mode)
{
    using RenderTargetImpl::equationToGlConstant;
    using RenderTargetImpl::factorToGlConstant;

    // Apply the blend mode
    glCheck(glBlendFuncSeparate(factorToGlConstant(mode.colorSrcFactor),
                                      factorToGlConstant(mode.colorDstFactor),
                                      factorToGlConstant(mode.alphaSrcFactor),
                                      factorToGlConstant(mode.alphaDstFactor)));


    glCheck(glBlendEquationSeparate(equationToGlConstant(mode.colorEquation),
                                          equationToGlConstant(mode.alphaEquation)));
}


////////////////////////////////////////////////////////////
RenderTarget::StepState::StepState(PrimitiveType ptype, const RenderStates& rstates)
: type(ptype)
, blendMode(rstates.blendMode)
, texture(rstates.texture)
, shader(rstates.shader)
{}


////////////////////////////////////////////////////////////
bool RenderTarget::StepState::operator==(const StepState& other) const
{
    return type == other.type &&
           blendMode == other.blendMode &&
           texture == other.texture &&
           shader == other.shader;
}


////////////////////////////////////////////////////////////
bool RenderTarget::StepState::operator!=(const StepState& other) const
{
    return !(*this == other);
}


////////////////////////////////////////////////////////////
RenderTarget::DrawStep::DrawStep(StepState state)
: state(state)
, vbo{0}
, vao{0}
, ebo{0}
, overruled{false}
{}


////////////////////////////////////////////////////////////
RenderTarget::DrawStep::DrawStep()
: state{PrimitiveType::Points, RenderStates::Default}
, vbo{0}
, vao{0}
, ebo{0}
, overruled{false}
{}


////////////////////////////////////////////////////////////
RenderTarget::DrawStep::DrawStep(DrawStep&& right)
: state{right.state}
, vertices{right.vertices}
, elements{right.elements}
, vbo{right.vbo}
, ebo{right.ebo}
, vao{right.vao}
, overruled{right.overruled}
{
    right.vbo = 0;
    right.ebo = 0;
    right.vao = 0;
}


////////////////////////////////////////////////////////////
RenderTarget::DrawStep& RenderTarget::DrawStep::operator=(DrawStep&& right) {
    state = right.state;
    vertices = right.vertices;
    elements = right.elements;
    vbo = right.vbo;
    ebo = right.ebo;
    vao = right.vao;
    overruled = right.overruled;

    right.vbo = 0;
    right.ebo = 0;
    right.vao = 0;

    return *this;
}


////////////////////////////////////////////////////////////
RenderTarget::DrawStep::~DrawStep()
{
    if (vbo) {
        glCheck(glDeleteBuffers(1, &vbo));
    }

    if (ebo) {
        glCheck(glDeleteBuffers(1, &ebo));
    }

    if (vao) {
        glCheck(glDeleteVertexArrays(1, &vao));
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::DrawStep::upload() {
    if (vbo) {
        glCheck(glDeleteBuffers(1, &vbo));
    }
    glCheck(glGenBuffers(1, &vbo));

    if (ebo) {
        glCheck(glDeleteBuffers(1, &ebo));
    }
    glCheck(glGenBuffers(1, &ebo));

    if (vao) {
        glCheck(glDeleteVertexArrays(1, &vao));
    }
    glCheck(glGenVertexArrays(1, &vao));
    glCheck(glBindVertexArray(vao));

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW));

    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size() * sizeof(unsigned int), elements.data(), GL_DYNAMIC_DRAW));

    glCheck(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(0)));
    glCheck(glEnableVertexAttribArray(0));
    glCheck(glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(2)));
    glCheck(glEnableVertexAttribArray(1));
    glCheck(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(6)));
    glCheck(glEnableVertexAttribArray(2));
}


////////////////////////////////////////////////////////////
void RenderTarget::clearOngoingStep() {
    if (m_steps.size() > m_stepsIdx) {
        if (m_steps[m_stepsIdx].state == m_currentStep.state &&
            m_steps[m_stepsIdx].vertices == m_currentStep.vertices &&
            m_steps[m_stepsIdx].elements == m_currentStep.elements) {
            // contents already within, no need to upload anything

            m_currentStep = DrawStep{};
            m_stepsIdx++;

            // skip over overruled steps
            while (m_steps.size() > m_stepsIdx && m_steps[m_stepsIdx].overruled) {
                m_stepsIdx++;
            }
        } else if (m_currentStep.vertices.size() && m_currentStep.elements.size()) {
            // contents dont match, reset and push
            // TODO cache instead of discard

            m_steps.erase(m_steps.begin() + static_cast<int>(m_stepsIdx), m_steps.end());
        }
    }

    if (m_currentStep.vertices.size() && m_currentStep.elements.size()) {
        m_currentStep.upload();

        m_steps.emplace_back(std::move(m_currentStep));
        m_currentStep = DrawStep{};
        m_stepsIdx++;
    }
}


} // namespace sf
