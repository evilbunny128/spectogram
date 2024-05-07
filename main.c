#include <stdio.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <complex.h>
#include <time.h>

#define ALSA_PCM_NEW_HW_PARAMS_API

#define SAMPLE_RATE 48000
#define N_SAMPLES 1024
#define N_FREQUENCIES 60

// given as one array with base frequencies stored in frequencies.
float complex *create_ft_matrix(float *frequencies);

// Apply transform matrix & absolute values
void find_frequency_amplitudes(float* samples,
			       float complex* ft_matrix,
			       float* freq_amplitudes);

float *make_frequency_space(float base_frequency);


// Do I need to use a radically higher sampling rate to get good resolution for my fourier transform?
int main(void) {
  // test setup to see if frequency f is detected properly
  float f;
  printf("What frequency should the algorithm try to re-create: ");
  if (scanf("%f", &f) != 1) {
    fprintf(stderr, "could not read user input test frequency\n");
    exit(0);
  }

  float* fs = make_frequency_space(52.5);
  float complex *ft_matrix = create_ft_matrix(fs);
  // make example signal
  float *buf = malloc(N_SAMPLES * sizeof(float));
  for (int t_idx = 0; t_idx < N_SAMPLES; t_idx++) {
    buf[t_idx] = cos(2 * M_PI * ((float)t_idx / SAMPLE_RATE) * f);
  }

  
  float* freq_amplitudes = malloc(N_FREQUENCIES * sizeof(float));
  clock_t start = clock();
  find_frequency_amplitudes(buf, ft_matrix, freq_amplitudes);
  clock_t end = clock();
  for (int f_idx = 0; f_idx < N_FREQUENCIES; f_idx++) {
    printf("frequency component %.1f magnitude: \t%.4f\n", fs[f_idx], freq_amplitudes[f_idx]);
  }
  printf("---\n  Time to compute fourier transform: %fms\n", 1000 * (float)(end - start) / CLOCKS_PER_SEC);

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
    // re-scale & clamp into something that we can use later for deciding size of boxes that we'll draw.
    // maybe add 30, divide by 25 and clamp to [0, 1]?
  }
}

float *make_frequency_space(float base_frequency) {
  float* fs = malloc(N_FREQUENCIES * sizeof(float));
  for (int f_idx = 0; f_idx < N_FREQUENCIES; f_idx++) {
    fs[f_idx] = base_frequency * pow(2, f_idx / 12.0);
  }
  return fs;
}
