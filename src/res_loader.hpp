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
//   ResLoadWav     -> Mix_Chunk*   (decoded immediately; RWops freed on return)
#pragma once

#include <string>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

TTF_Font* ResOpenFont(const std::string& path, int px);
SDL_Texture* ResLoadTexture(SDL_Renderer* renderer, const std::string& path);
Mix_Chunk* ResLoadWav(const std::string& path);

// Two-phase WAV loading: the FILE read must happen before the audio device
// opens (SD reads pre-audio are proven safe); the Mix_Chunk decode needs the
// device open (SDL_mixer converts against the device format at load).
int  ResStageWav(const std::string& path);   // returns staged id, -1 on fail
Mix_Chunk* ResDecodeWav(int stagedId, uint32_t deviceRate); // after audio open; linear-resamples to deviceRate

// Frees the font buffers + RWops.  Call once, after every TTF_Font obtained
// from ResOpenFont has been closed with TTF_CloseFont.
void ResShutdown();
