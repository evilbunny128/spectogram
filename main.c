#include <stdio.h>
#include <stdlib.h>

#include <math.h>
#include <complex.h>

#include <SDL2/SDL.h>

#include <pulse/simple.h>
#include <pulse/error.h>

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
  float* fs = make_frequency_space(52.5);
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
  s = pa_simple_new(NULL, "Spectogram",
		    PA_STREAM_RECORD, NULL,
		    "System sound output", &ss,
		    NULL, NULL, &error);
  if (!s) {
    fprintf(stderr, "pa_simple_new() failed: %s\n", pa_strerror(error));
    pa_simple_free(s);
    return 1;
  }

  // TODO: setup window to render in

  float* buf = malloc(N_SAMPLES * sizeof(float));
  while (1) {
    if (pa_simple_read(s, buf, N_SAMPLES * sizeof(float), &error) < 0) {
      fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
      goto finish;
    }

    find_frequency_amplitudes(buf, ft_matrix, freq_amplitudes);
    // TODO: Render graphically
  }
  
  // put inside loop
  find_frequency_amplitudes(buf, ft_matrix, freq_amplitudes);

finish:
  free(buf);
  pa_simple_free(s);
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
