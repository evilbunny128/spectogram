#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <time.h>

#include <math.h>
#include <complex.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#define SAMPLE_RATE 48000
#define N_SAMPLES 4096
#define N_FREQUENCIES 120
#define LISTENING_LENGTH 256

// given as one array with base frequencies stored in frequencies.
float complex *create_ft_matrix(float *frequencies);

// Apply transform matrix & absolute values
void find_frequency_amplitudes(float* samples,
			       float complex* ft_matrix,
			       float* freq_amplitudes);

float *make_frequency_space(float base_frequency);

int get_column_pos(int idx);
int get_pixel_height(float ampl_dB);

int window_height = 600;
int window_width = 800;

void render_frequencies(SDL_Renderer* renderer, float* freq_amplitudes);


int main(void) {
  float* fs = make_frequency_space(105.);
  float complex *ft_matrix = create_ft_matrix(fs);
  float* freq_amplitudes = malloc(N_FREQUENCIES * sizeof(float));

  
  // Specify sound format & create loopback device we can read from
  static const pa_sample_spec ss = {
    .format = PA_SAMPLE_FLOAT32LE,
    .rate = SAMPLE_RATE,
    .channels = 1
  };
  pa_simple *s;
  int error;

  pa_buffer_attr buff_attr = {
    .fragsize = LISTENING_LENGTH * sizeof(float),
    .minreq = -1,
    .prebuf = -1,
    .tlength = -1,
    .maxlength = -1
  };
  
  s = pa_simple_new(NULL, "Spectogram",
		    PA_STREAM_RECORD, NULL,
		    "System sound output", &ss,
		    NULL, &buff_attr, &error);
  if (!s) {
    fprintf(stderr, "pa_simple_new() failed: %s\n", pa_strerror(error));
    pa_simple_free(s);
    return 1;
  }

  // TODO: setup window to render in
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Event event;
  
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    SDL_Log("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    goto finish;
  }

  window = SDL_CreateWindow("Spectogram",
			    SDL_WINDOWPOS_UNDEFINED,
			    SDL_WINDOWPOS_UNDEFINED,
			    window_width,
			    window_height,
			    SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    SDL_Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    goto finish;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (renderer == NULL) {
    SDL_Log("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
    goto finish;
  }


  float* buf = malloc(N_SAMPLES * sizeof(float)); 
  bool quit = false;
  while (!quit) {
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
	quit = true;
      }
      else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
	SDL_GetWindowSize(window, &window_width, &window_height);
      }
    }

    if (pa_simple_read(s, &buf[3 * LISTENING_LENGTH], LISTENING_LENGTH * sizeof(float), &error) < 0) {
      fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
      goto finish;
    }
    
    find_frequency_amplitudes(buf, ft_matrix, freq_amplitudes);
    memmove(buf, &buf[LISTENING_LENGTH], sizeof(float) * (N_SAMPLES - LISTENING_LENGTH));
    
    render_frequencies(renderer, freq_amplitudes);
  }
  
finish:
  pa_simple_free(s);

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  
  free(buf);
  free(freq_amplitudes);
  free(fs);
  free(ft_matrix);

  return 0;
}


float complex *create_ft_matrix(float *frequencies) {
  float complex *matrix = malloc(N_SAMPLES * N_FREQUENCIES * sizeof(float complex));
  float dt = 1.0 / SAMPLE_RATE;
  for (int f_idx = 0; f_idx < N_FREQUENCIES; f_idx++) {
    for (int t_idx = 0; t_idx < N_SAMPLES; t_idx++) {
      float t = (float)t_idx * dt;
      float f = frequencies[f_idx];
      matrix[f_idx * N_SAMPLES + t_idx] = cexp(-I * t * f * 2 * M_PI) / (float)N_SAMPLES;
    }
  }
  return matrix;
}


int get_column_pos(int idx) {
  float fraction_possition = 1. / ((float)N_FREQUENCIES * 2) + (float)idx * 1. / ((float)N_FREQUENCIES);
  return (int)(fraction_possition * (float)window_width);
}

int get_pixel_height(float ampl_dB) {
  // shift [-55, -5] -> [0, 1]
  ampl_dB += 55.;
  ampl_dB /= 50.;
  ampl_dB = ampl_dB < 0. ? 0. : ampl_dB;
  ampl_dB = 1. < ampl_dB ? 1. : ampl_dB;
  return (int)((float)window_height * (1. - ampl_dB));
}

void find_frequency_amplitudes(float* samples,
			       float complex* ft_matrix,
			       float* freq_amplitudes) {
  // I have a feeling this order of operations and memory layout is not the optimal for the number of memory accesses.
  // This way is still better in terms of understanding what is happening and we don't need to deal with a large
  // complex accumulator array to then take absolute values of, so this is probably fine. Either way, Matrix should fit in L2 comfortably.
  for (int f_idx = 0; f_idx < N_FREQUENCIES; f_idx++) {
    float complex amplitude = 0;
    for (int t_idx = 0; t_idx < N_SAMPLES; t_idx++) {
      amplitude += samples[t_idx] * ft_matrix[f_idx * N_SAMPLES + t_idx];
    }
    freq_amplitudes[f_idx] = 10 * log10(amplitude * conj(amplitude)); // use dB scale
    // clamping & re-scaling done when rendering.
  }
}

float *make_frequency_space(float base_frequency) {
  float* fs = malloc(N_FREQUENCIES * sizeof(float));
  for (int f_idx = 0; f_idx < N_FREQUENCIES; f_idx++) {
    fs[f_idx] = base_frequency * pow(2, f_idx / 24.0);
  }
  return fs;
}


void render_frequencies(SDL_Renderer* renderer, float* freq_amplitudes) {
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  SDL_Rect rect[N_FREQUENCIES];
  for (int freq_idx = 0; freq_idx < N_FREQUENCIES; freq_idx++) {
    rect[freq_idx].x = get_column_pos(freq_idx);
    rect[freq_idx].y = get_pixel_height(freq_amplitudes[freq_idx]);
    rect[freq_idx].h = window_height - get_pixel_height(freq_amplitudes[freq_idx]);
    rect[freq_idx].w = window_width / (N_FREQUENCIES * 2);

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // change later to vary with frequency.

    SDL_RenderFillRect(renderer, &rect[freq_idx]);
  }
  SDL_RenderPresent(renderer);
}

