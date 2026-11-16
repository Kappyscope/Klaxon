// res_loader.hpp — memory-backed asset loading.
//
// On real hardware, streaming reads from /vol/content (the .wuhb content
// mount) hard-hang inside freetype (TTF_OpenFont never returns).  fopen on
// the same paths works fine, so we read every asset fully into memory once
// and back SDL's decoders with memory RWops.  No filesystem I/O after boot.
//
//   ResOpenFont    -> TTF_Font*   (buffer + RWops kept alive for the font's
//                                  lifetime; release with ResShutdown AFTER
//                                  all fonts are closed via TTF_CloseFont)
//   ResLoadTexture -> SDL_Texture* (decoded immediately; RWops freed on return)
//
// Sound effects are decoded by us into plain PCM (SfxClip) — SDL_mixer's
// decoder takes the audio-device lock, which deadlocks on this port.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

// Interleaved S16 stereo at the audio device's rate.
struct SfxClip {
    std::vector<int16_t> pcm;
    uint32_t frames = 0;
};

TTF_Font* ResOpenFont(const std::string& path, int px);
SDL_Texture* ResLoadTexture(SDL_Renderer* renderer, const std::string& path);

// Two-phase WAV loading: the FILE read must happen before the audio device
// opens (SD reads pre-audio are proven safe); decoding is pure memory work
// and produces PCM at deviceRate.
int  ResStageWav(const std::string& path);   // returns staged id, -1 on fail
bool ResDecodeWav(int stagedId, uint32_t deviceRate, SfxClip* out);

// Frees the font buffers + RWops.  Call once, after every TTF_Font obtained
// from ResOpenFont has been closed with TTF_CloseFont.
void ResShutdown();
