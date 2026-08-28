#pragma once

#include <JuceHeader.h>
#include "AudioState.h"

class OpenGLScopeView final : public juce::Component, private juce::OpenGLRenderer
{
public:
    explicit OpenGLScopeView(AudioState& state) : audioState(state)
    {
        setOpaque(true);
        openGLContext.setRenderer(this);
        openGLContext.setComponentPaintingEnabled(true);
        openGLContext.setContinuousRepainting(true);
        openGLContext.setOpenGLVersionRequired(juce::OpenGLContext::defaultGLVersion);
        openGLContext.attachTo(*this);
    }

    ~OpenGLScopeView() override
    {
        openGLContext.detach();
    }

    void setRenderData(const std::vector<float>& values)
    {
        const juce::ScopedLock lock(dataLock);
        pendingSpectrum = values;
        hasPendingData = true;
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        if (bounds.isEmpty())
            return;

        constexpr std::array<float, 10> freqs
        {
            20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
            1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f
        };

        constexpr std::array<float, 24> fineFreqs
        {
            30.0f, 40.0f, 60.0f, 70.0f, 80.0f, 90.0f,
            300.0f, 400.0f, 600.0f, 700.0f, 800.0f, 900.0f,
            3000.0f, 4000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f,
            12000.0f, 13000.0f, 14000.0f, 16000.0f, 17000.0f, 18000.0f
        };

        const float axisY = bounds.getBottom() - axisOffsetPixels;

        g.setColour(juce::Colour::fromFloatRGBA(0.24f, 1.0f, 0.42f, 0.07f));
        for (float f : fineFreqs)
        {
            const float x = frequencyToX(f, bounds.getWidth()) + bounds.getX();
            const float dashLengths[] = { 2.0f, 5.0f };
            g.drawDashedLine(juce::Line<float>(x, bounds.getY() + 10.0f, x, axisY - 2.0f), dashLengths, 2, 0.8f);
        }

        g.setColour(juce::Colour::fromFloatRGBA(0.26f, 1.0f, 0.44f, 0.48f));
        g.drawHorizontalLine((int)axisY, bounds.getX(), bounds.getRight());

        struct LabelCandidate
        {
            float x = 0.0f;
            juce::String text;
            juce::Colour colour;
            float width = 0.0f;
            int priority = 0;
        };

        std::vector<LabelCandidate> candidates;
        candidates.reserve(freqs.size() + fineFreqs.size());

        const juce::Font labelFont(12.0f, juce::Font::plain);
        auto toLabel = [](float f)
        {
            if (f < 1000.0f)
                return juce::String((int)f);
            if (std::fmod(f, 1000.0f) == 0.0f)
                return juce::String((int)(f / 1000.0f)) + "k";
            return juce::String(f / 1000.0f, 1) + "k";
        };

        auto addLabelCandidate = [&](float f, bool strongLine, int priority)
        {
            const float x = frequencyToX(f, bounds.getWidth()) + bounds.getX();
            const juce::String text = toLabel(f);
            float width = (float)(text.length() * 9 + 14);
            if (width < 34.0f)
                width = 34.0f;
            candidates.push_back({
                x,
                text,
                strongLine
                    ? juce::Colour::fromFloatRGBA(0.84f, 1.0f, 0.88f, 0.88f)
                    : juce::Colour::fromFloatRGBA(0.72f, 1.0f, 0.80f, 0.72f),
                width,
                priority
            });
        };

        for (float f : fineFreqs)
        {
            if (f <= 5000.0f)
                addLabelCandidate(f, false, 1);
        }

        for (float f : freqs)
        {
            const float x = frequencyToX(f, bounds.getWidth()) + bounds.getX();
            const bool strongLine = (f == 100.0f || f == 1000.0f || f == 10000.0f);

            if (strongLine)
            {
                g.setColour(juce::Colour::fromFloatRGBA(0.34f, 1.0f, 0.52f, 0.34f));
                g.drawLine(juce::Line<float>(x, bounds.getY() + 8.0f, x, axisY - 2.0f), 1.4f);
            }
            else
            {
                g.setColour(juce::Colour::fromFloatRGBA(0.26f, 1.0f, 0.42f, 0.13f));
                const float dashLengths[] = { 3.0f, 4.0f };
                g.drawDashedLine(juce::Line<float>(x, bounds.getY() + 8.0f, x, axisY - 2.0f), dashLengths, 2, 0.9f);
            }

            g.setColour(strongLine
                ? juce::Colour::fromFloatRGBA(0.42f, 1.0f, 0.60f, 0.90f)
                : juce::Colour::fromFloatRGBA(0.34f, 1.0f, 0.54f, 0.72f));
            g.drawVerticalLine((int)x, axisY - 5.0f, axisY + 4.0f);

            if (f <= 5000.0f || f == 10000.0f)
                addLabelCandidate(f, strongLine, strongLine ? 3 : 2);
        }

        std::sort(candidates.begin(), candidates.end(), [](const LabelCandidate& a, const LabelCandidate& b)
            {
                if (a.priority != b.priority)
                    return a.priority > b.priority;
                return a.x < b.x;
            });

        std::vector<LabelCandidate> accepted;
        accepted.reserve(candidates.size());
        const float minGap = 6.0f;

        for (const auto& c : candidates)
        {
            const float left = c.x - c.width * 0.5f;
            const float right = c.x + c.width * 0.5f;
            bool overlaps = false;

            for (const auto& a : accepted)
            {
                const float aLeft = a.x - a.width * 0.5f;
                const float aRight = a.x + a.width * 0.5f;
                if (!(right + minGap < aLeft || left > aRight + minGap))
                {
                    overlaps = true;
                    break;
                }
            }

            if (!overlaps)
                accepted.push_back(c);
        }

        std::sort(accepted.begin(), accepted.end(), [](const LabelCandidate& a, const LabelCandidate& b)
            {
                return a.x < b.x;
            });

        g.setFont(labelFont);
        for (const auto& c : accepted)
        {
            g.setColour(c.colour);
            g.drawText(c.text, (int)std::round(c.x - c.width * 0.5f), (int)axisY + 7, (int)std::round(c.width), 16, juce::Justification::centred);
        }

        g.setColour(juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.45f));
        g.fillRoundedRectangle(bounds.getRight() - 38.0f, axisY + 6.0f, 34.0f, 16.0f, 3.0f);
        g.setColour(juce::Colour::fromFloatRGBA(0.72f, 1.0f, 0.80f, 0.98f));
        g.setFont(juce::Font(12.5f, juce::Font::bold));
        g.drawText("Hz", (int)bounds.getRight() - 35, (int)axisY + 7, 28, 14, juce::Justification::centred);
    }
    void resized() override {}

private:
    struct LineVertex { float x; float y; };
    struct ParticleVertex { float x; float y; float alpha; };
    struct Particle { float x; float y; float vy; float alpha; };

    static constexpr const char* vertexShader = R"glsl(
        attribute vec2 position;
        attribute float alphaIn;
        uniform float pointSize;
        uniform float pointMode;
        varying float vAlpha;
        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
            gl_PointSize = pointSize;
            vAlpha = pointMode > 0.5 ? alphaIn : 1.0;
        }
    )glsl";

    static constexpr const char* fragmentShader = R"glsl(
        uniform vec4 colour;
        uniform float pointMode;
        uniform float particleHardMode;
        varying float vAlpha;
        void main() {
            if (pointMode > 0.5) {
                if (particleHardMode > 0.5) {
                    gl_FragColor = vec4(colour.rgb, colour.a * vAlpha);
                    return;
                }
                vec2 p = gl_PointCoord - vec2(0.5);
                float d = length(p) * 2.0;
                float roundMask = clamp(1.0 - d * d, 0.0, 1.0);
                float alpha = (0.18 + 0.82 * roundMask) * vAlpha;
                gl_FragColor = vec4(colour.rgb, colour.a * alpha);
            } else {
                gl_FragColor = vec4(colour.rgb, colour.a * vAlpha);
            }
        }
    )glsl";

    void newOpenGLContextCreated() override
    {
        shader = std::make_unique<juce::OpenGLShaderProgram>(openGLContext);
        if (!shader->addVertexShader(vertexShader)
            || !shader->addFragmentShader(fragmentShader)
            || !shader->link())
        {
            DBG("OpenGLScopeView shader error: " << shader->getLastError());
            shader.reset();
            return;
        }

        positionAttribute = std::make_unique<juce::OpenGLShaderProgram::Attribute>(*shader, "position");
        alphaAttribute = std::make_unique<juce::OpenGLShaderProgram::Attribute>(*shader, "alphaIn");
        juce::gl::glEnable(juce::gl::GL_PROGRAM_POINT_SIZE);
        juce::gl::glGenBuffers(1, &lineBuffer);
        juce::gl::glGenBuffers(1, &particleBuffer);
    }

    void openGLContextClosing() override
    {
        if (lineBuffer != 0)
            juce::gl::glDeleteBuffers(1, &lineBuffer);
        if (particleBuffer != 0)
            juce::gl::glDeleteBuffers(1, &particleBuffer);
        lineBuffer = 0;
        particleBuffer = 0;
        alphaAttribute.reset();
        positionAttribute.reset();
        shader.reset();
    }

    void renderOpenGL() override
    {
        const bool particleVisibilityTestMode = audioState.particleVisibilityTestMode.load();
        const auto scale = (float)openGLContext.getRenderingScale();
        const int width = juce::jmax(1, juce::roundToInt((float)getWidth() * scale));
        const int height = juce::jmax(1, juce::roundToInt((float)getHeight() * scale));
        juce::gl::glViewport(0, 0, width, height);
        juce::gl::glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        juce::gl::glClear(juce::gl::GL_COLOR_BUFFER_BIT);

        if (shader == nullptr || positionAttribute == nullptr)
            return;

        copyPendingData();
        syncVisualParams();
        updateSpectrumVerticesAndSpawn(width, height, scale);
        updateParticles(height, scale);

        if (spectrumVertices.empty())
            return;

        uploadBuffer(lineBuffer, spectrumVertices);
        buildParticleVertices();
        uploadBuffer(particleBuffer, particleVertices);
        juce::gl::glEnable(juce::gl::GL_BLEND);
        juce::gl::glBlendFunc(juce::gl::GL_SRC_ALPHA, juce::gl::GL_ONE);
        shader->use();

        drawGlowPoints(scale);
        drawLine(0.20f, 1.0f, 0.28f, 0.95f, 2.2f);
        drawLine(0.88f, 1.0f, 0.90f, 1.0f, 1.4f);

        if (particleVisibilityTestMode)
            drawParticleDebugAnchors(scale);

        if (!particleVertices.empty())
        {
            shader->setUniform("pointMode", 1.0f);
            shader->setUniform("particleHardMode", particleVisibilityTestMode ? 1.0f : 0.0f);
            shader->setUniform("pointSize", juce::jmax(4.0f, particleRadius * 6.0f) * scale);
            shader->setUniform("colour", 0.22f, 1.0f, 0.42f, 0.32f + alphaGlow * 0.30f);
            drawBuffer(particleBuffer, juce::gl::GL_POINTS, (int)particleVertices.size());
            shader->setUniform("pointSize", juce::jmax(2.0f, particleRadius * 2.8f) * scale);
            shader->setUniform("colour", 0.78f, 1.0f, 0.84f, 0.92f);
            drawBuffer(particleBuffer, juce::gl::GL_POINTS, (int)particleVertices.size());
        }

        juce::gl::glDisable(juce::gl::GL_BLEND);
    }

    void copyPendingData()
    {
        const juce::ScopedLock lock(dataLock);
        if (!hasPendingData) return;

        currentSpectrum = pendingSpectrum;
        spectrumDirty = true;
        hasPendingData = false;
    }

    void syncVisualParams()
    {
        const float glow = audioState.glow.load();
        const float glowAmount = juce::jlimit(0.0f, 1.0f, audioState.glowAmount.load());
        const float frameMagnitude = getCurrentSpectrumPeak();
        const float glowScale = juce::jmap(glowAmount, 1.0f, juce::jlimit(0.0f, 1.0f, frameMagnitude));
        alphaGlow = juce::jlimit(0.0f, 1.0f, glow * glowScale);
        particleRadius = juce::jmax(0.1f, audioState.particleRadius.load());
    }

    float getCurrentSpectrumPeak() const
    {
        float peak = 0.0f;
        for (float v : currentSpectrum)
            peak = juce::jmax(peak, juce::jlimit(0.0f, 1.0f, v));
        return peak;
    }

    void updateSpectrumVerticesAndSpawn(int width, int height, float scale)
    {
        if (currentSpectrum.empty())
        {
            spectrumVertices.clear();
            return;
        }

        const float topPadPx = 8.0f * scale;
        const float plotBottomPx = juce::jmax(topPadPx + 1.0f, (float)height - (axisOffsetPixels + axisPlotGapPixels) * scale);
        spectrumTopNdc = 1.0f - 2.0f * (topPadPx / (float)height);
        spectrumFloorNdc = 1.0f - 2.0f * (plotBottomPx / (float)height);

        spectrumVertices.clear();
        spectrumVertices.reserve(currentSpectrum.size());
        for (size_t i = 0; i < currentSpectrum.size(); ++i)
        {
            const float x = currentSpectrum.size() > 1 ? (float)i / (float)(currentSpectrum.size() - 1) : 0.0f;
            const float level = juce::jlimit(0.0f, 1.0f, currentSpectrum[i]);
            const float yNdc = spectrumFloorNdc + level * (spectrumTopNdc - spectrumFloorNdc);
            spectrumVertices.push_back({x * 2.0f - 1.0f, yNdc});
        }

        if (!spectrumDirty || width <= 0 || height <= 0)
            return;

        spawnParticlesFromSpectrum(width, height);
        spectrumDirty = false;
    }

    void spawnParticlesFromSpectrum(int width, int height)
    {
        const int spawnStep = juce::jmax(1, audioState.particleSpawnStep.load());
        const size_t maxCount = (size_t)juce::jmax(1, audioState.particleMaxCount.load());
        const float initVyPx = juce::jmax(0.0f, audioState.particleInitVy.load());
        const float pxToNdcX = 2.0f / (float)juce::jmax(1, width);
        const float pxToNdcY = 2.0f / (float)juce::jmax(1, height);

        for (size_t i = 0; i < spectrumVertices.size(); i += (size_t)spawnStep)
        {
            if (particles.size() >= maxCount)
                break;

            const float jitter = (particleRandom.nextFloat() - 0.5f) * 2.0f;
            Particle p;
            p.x = spectrumVertices[i].x + jitter * (1.5f * pxToNdcX);
            p.y = spectrumVertices[i].y;
            p.vy = (initVyPx * (0.8f + particleRandom.nextFloat() * 0.4f)) * pxToNdcY;
            p.alpha = 1.0f;
            particles.push_back(p);
        }
    }

    void updateParticles(int height, float scale)
    {
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        if (lastFrameTimeMs <= 0.0)
        {
            lastFrameTimeMs = nowMs;
            return;
        }

        const float dt = juce::jlimit(0.0f, 0.05f, (float)((nowMs - lastFrameTimeMs) * 0.001));
        lastFrameTimeMs = nowMs;
        if (dt <= 0.0f || particles.empty())
            return;

        const float pxToNdcY = 2.0f / (float)juce::jmax(1, height);
        const float gravity = -audioState.particleGravity.load() * pxToNdcY;
        const float fadeRate = juce::jmax(0.0f, audioState.particleFadeRate.load());
        const float lowerClipNdc = spectrumFloorNdc - (2.0f * scale / (float)juce::jmax(1, height));

        for (size_t i = 0; i < particles.size();)
        {
            auto& p = particles[i];
            p.vy += gravity * dt;
            p.y += p.vy * dt;
            p.alpha -= fadeRate * dt;

            if (p.alpha <= 0.0f || p.y < lowerClipNdc || p.y > 1.1f)
            {
                particles[i] = particles.back();
                particles.pop_back();
            }
            else
            {
                ++i;
            }
        }
    }

    void buildParticleVertices()
    {
        particleVertices.clear();
        particleVertices.reserve(particles.size());
        for (const auto& p : particles)
        {
            if (p.y < spectrumFloorNdc)
                continue;
            particleVertices.push_back({p.x, p.y, juce::jlimit(0.08f, 1.0f, p.alpha)});
        }
    }

    static float frequencyToX(float freq, float width)
    {
        constexpr float minFreq = 20.0f;
        constexpr float maxFreq = 20000.0f;
        const float norm = (std::log10(freq) - std::log10(minFreq)) / (std::log10(maxFreq) - std::log10(minFreq));
        return juce::jlimit(0.0f, width, norm * width);
    }

    void uploadBuffer(GLuint buffer, const std::vector<LineVertex>& vertices)
    {
        juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, buffer);
        juce::gl::glBufferData(juce::gl::GL_ARRAY_BUFFER,
            (GLsizeiptr)(vertices.size() * sizeof(LineVertex)),
            vertices.data(),
            juce::gl::GL_DYNAMIC_DRAW);
    }

    void uploadBuffer(GLuint buffer, const std::vector<ParticleVertex>& vertices)
    {
        juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, buffer);
        juce::gl::glBufferData(juce::gl::GL_ARRAY_BUFFER,
            (GLsizeiptr)(vertices.size() * sizeof(ParticleVertex)),
            vertices.data(),
            juce::gl::GL_DYNAMIC_DRAW);
    }

    void drawLine(float red, float green, float blue, float alpha, float lineWidth)
    {
        shader->setUniform("pointMode", 0.0f);
        shader->setUniform("particleHardMode", 0.0f);
        shader->setUniform("pointSize", 1.0f);
        shader->setUniform("colour", red, green, blue, alpha * (0.2f + alphaGlow * 0.8f));
        juce::gl::glLineWidth(lineWidth);
        drawLineBuffer(lineBuffer, juce::gl::GL_LINE_STRIP, (int)spectrumVertices.size());
    }

    void drawGlowPoints(float scale)
    {
        const float glowStrength = 0.25f + alphaGlow * 0.75f;

        shader->setUniform("pointMode", 1.0f);
        shader->setUniform("particleHardMode", 0.0f);
        shader->setUniform("pointSize", juce::jmax(2.0f, 18.0f * scale));
        shader->setUniform("colour", 0.00f, 0.48f, 0.08f, 0.14f * glowStrength);
        drawLineBuffer(lineBuffer, juce::gl::GL_POINTS, (int)spectrumVertices.size());

        shader->setUniform("pointSize", juce::jmax(1.0f, 10.0f * scale));
        shader->setUniform("colour", 0.10f, 0.90f, 0.18f, 0.24f * glowStrength);
        drawLineBuffer(lineBuffer, juce::gl::GL_POINTS, (int)spectrumVertices.size());

        shader->setUniform("pointSize", juce::jmax(1.0f, 6.0f * scale));
        shader->setUniform("colour", 0.45f, 1.0f, 0.56f, 0.30f * glowStrength);
        drawLineBuffer(lineBuffer, juce::gl::GL_POINTS, (int)spectrumVertices.size());
    }

    void drawParticleDebugAnchors(float scale)
    {
        shader->setUniform("pointMode", 1.0f);
        shader->setUniform("particleHardMode", 1.0f);
        shader->setUniform("pointSize", juce::jmax(3.0f, 5.0f * scale));
        shader->setUniform("colour", 1.0f, 0.35f, 0.15f, 0.9f);
        drawLineBuffer(lineBuffer, juce::gl::GL_POINTS, (int)spectrumVertices.size());
    }

    void drawLineBuffer(GLuint buffer, GLenum primitive, int count)
    {
        juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, buffer);
        juce::gl::glEnableVertexAttribArray(positionAttribute->attributeID);
        juce::gl::glVertexAttribPointer(positionAttribute->attributeID, 2, juce::gl::GL_FLOAT,
            juce::gl::GL_FALSE, sizeof(LineVertex), nullptr);
        if (alphaAttribute != nullptr && alphaAttribute->attributeID >= 0)
        {
            juce::gl::glDisableVertexAttribArray(alphaAttribute->attributeID);
            juce::gl::glVertexAttrib1f(alphaAttribute->attributeID, 1.0f);
        }
        juce::gl::glDrawArrays(primitive, 0, count);
        juce::gl::glDisableVertexAttribArray(positionAttribute->attributeID);
        juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, 0);
    }

    void drawBuffer(GLuint buffer, GLenum primitive, int count)
    {
        juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, buffer);
        juce::gl::glEnableVertexAttribArray(positionAttribute->attributeID);
        juce::gl::glVertexAttribPointer(positionAttribute->attributeID, 2, juce::gl::GL_FLOAT,
            juce::gl::GL_FALSE, sizeof(ParticleVertex), nullptr);
        if (alphaAttribute != nullptr && alphaAttribute->attributeID >= 0)
        {
            juce::gl::glEnableVertexAttribArray(alphaAttribute->attributeID);
            juce::gl::glVertexAttribPointer(alphaAttribute->attributeID, 1, juce::gl::GL_FLOAT,
                juce::gl::GL_FALSE, sizeof(ParticleVertex),
                (const void*)offsetof(ParticleVertex, alpha));
        }
        juce::gl::glDrawArrays(primitive, 0, count);
        if (alphaAttribute != nullptr && alphaAttribute->attributeID >= 0)
            juce::gl::glDisableVertexAttribArray(alphaAttribute->attributeID);
        juce::gl::glDisableVertexAttribArray(positionAttribute->attributeID);
        juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, 0);
    }

    AudioState& audioState;
    juce::OpenGLContext openGLContext;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> positionAttribute;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> alphaAttribute;
    GLuint lineBuffer{0};
    GLuint particleBuffer{0};
    juce::CriticalSection dataLock;
    std::vector<float> pendingSpectrum;
    std::vector<float> currentSpectrum;
    std::vector<LineVertex> spectrumVertices;
    std::vector<ParticleVertex> particleVertices;
    std::vector<Particle> particles;
    juce::Random particleRandom;
    bool spectrumDirty{false};
    double lastFrameTimeMs{0.0};
    float alphaGlow{0.7f};
    float particleRadius{1.0f};
    float spectrumFloorNdc{-0.85f};
    float spectrumTopNdc{0.98f};
    static constexpr float axisOffsetPixels = 34.0f;
    static constexpr float axisPlotGapPixels = 6.0f;
    bool hasPendingData{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGLScopeView)
};
