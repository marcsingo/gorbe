#ifndef GORBE_FRAMEBUFFER_HPP
#define GORBE_FRAMEBUFFER_HPP

#include <algorithm>
#include <glad/glad.h>

// Képernyőn kívüli rajzolási cél: színtextúra + mélység-pufferrel.
//
// A jelenet ide rajzolódik, a textúrát pedig az ImGui teszi ki egy ablakba
// (ImGui::Image). Enélkül a 3D kép nem lehetne egy panel belsejében.
class Framebuffer {
    GLuint fbo_ = 0;
    GLuint tex_ = 0;
    GLuint rbo_ = 0;
    int    w_   = 0;
    int    h_   = 0;

    void destroy() {
        if (rbo_) { glDeleteRenderbuffers(1, &rbo_); rbo_ = 0; }
        if (tex_) { glDeleteTextures(1, &tex_);      tex_ = 0; }
        if (fbo_) { glDeleteFramebuffers(1, &fbo_);  fbo_ = 0; }
        w_ = h_ = 0;
    }

public:
    Framebuffer() = default;
    ~Framebuffer() { destroy(); }

    Framebuffer(Framebuffer const&) = delete;
    Framebuffer& operator=(Framebuffer const&) = delete;

    int    width()   const { return w_; }
    int    height()  const { return h_; }
    GLuint texture() const { return tex_; }
    bool   valid()   const { return fbo_ != 0 && w_ > 0 && h_ > 0; }

    // Csak akkor épít újra, ha tényleg változott a méret — a textúra azonosítója
    // különben minden frame-ben más lenne, és az ImGui egy már törölt textúrára
    // hivatkozna.
    void resize(int nw, int nh) {
        nw = std::clamp(nw, 1, 8192);
        nh = std::clamp(nh, 1, 8192);
        if (nw == w_ && nh == h_ && fbo_ != 0) return;

        destroy();
        w_ = nw;
        h_ = nh;

        glGenFramebuffers(1, &fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

        glGenTextures(1, &tex_);
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w_, h_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // Enélkül a szélén a szomszédos oldalról vett minta csíkot húzna.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, 0);

        // A mélységi teszthez renderbuffer elég: sosem olvassuk vissza.
        glGenRenderbuffers(1, &rbo_);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w_, h_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_);

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void bind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, w_, h_);
    }

    static void unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
};

#endif //GORBE_FRAMEBUFFER_HPP
