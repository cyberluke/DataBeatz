#!/usr/bin/python3
#-*- encoding: Utf-8 -*-

import numpy as np
from scipy.signal import find_peaks, correlate, butter, filtfilt
from scipy.spatial.distance import euclidean
from fastdtw import fastdtw
from statistics import mean

class AudioRecognition:
    def __init__(self, audio_buffer_local, audio_buffer_remote):
        self.audio_data_local = np.array(audio_buffer_local)
        self.audio_data_remote = np.array(audio_buffer_remote)
        self.fs = 16000

    def find_peak_indices_height(self, signal, height):
        peaks, _ = find_peaks(signal, height=height)
        return peaks
    
    def low_pass_filter(self, data, cutoff=1000, fs=16000, order=4):
        b, a = butter(order, cutoff / (0.5 * fs), btype='low', analog=False)
        y = filtfilt(b, a, data)
        return y
    
    def find_peak_indices(self, signal):
        # Dynamic thresholding based on 80% of the highest peak
        highest_peak = np.max(np.abs(signal))
        threshold = 0.9 * highest_peak
        peaks, _ = find_peaks(signal, height=threshold)
        return peaks

    def fft_spectrum(self, signal):
        return np.abs(np.fft.fft(signal))

    # Dynamic Time Warping
    def compute_dtw(self, x, y):
        distance, path = fastdtw(x, y, dist=euclidean)
        return distance, path

    def get_average_delay(self):
        try:
            # Step 1: Peak Detection
            peak_indices_local = self.find_peak_indices(self.audio_data_local)
            peak_indices_remote = self.find_peak_indices(self.audio_data_remote)

            if len(peak_indices_local) == 0 or len(peak_indices_remote) == 0:
                print("No peaks found.")
                return None

            # Step 2: Dynamic Time Warping
            distance, path = self.compute_dtw(peak_indices_local, peak_indices_remote)

            # Compute the average delay
            delay_diffs = [y - x for (x, y) in path]
            avg_delay_ms = np.mean(delay_diffs) * (1000 / self.fs)
            
            return avg_delay_ms

        except Exception as e:
            print("An error occurred: {}".format(e))
            return None
        
    def find_audio_delay(self):
        # Step 1: Peak Detection
        peak_indices_local = self.find_peak_indices(self.audio_data_local)
        peak_indices_remote = self.find_peak_indices(self.audio_data_remote)

        # Step 2: Spectral Features
        fft_local = self.fft_spectrum(self.audio_data_local)
        fft_remote = self.fft_spectrum(self.audio_data_remote)

        # Debugging
        print("fft_local shape:", fft_local.shape)
        print("fft_remote shape:", fft_remote.shape)
        print("Peak indices local:", peak_indices_local)
        print("Peak indices remote:", peak_indices_remote)

        if len(peak_indices_local) == 0 or len(peak_indices_remote) == 0:
            print("No peaks found.")
            return None
        
        # Step 3: Guided Correlation around Peaks
        lag = []
        for peak in peak_indices_local:
            #print("Current peak:", peak)
            window = 100  # This is a hyperparameter you'd need to tune

            # Check for boundary conditions
            if peak < window or peak + window >= len(fft_local):
                print("Skipping peak", peak, "due to boundary conditions.")
                continue

            sub_seq_local = fft_local[peak - window: peak + window]
            sub_seq_remote = fft_remote[peak - window: peak + window]
            
            # Debugging
            #print("sub_seq_local length:", len(sub_seq_local))
            #print("sub_seq_remote length:", len(sub_seq_remote))
            
            if len(sub_seq_local) == 0 or len(sub_seq_remote) == 0:
                print("Skipping due to empty sub-sequence.")
                continue

            corr = correlate(sub_seq_local, sub_seq_remote)
            lag.append(np.argmax(corr))


        # If the lag crosses some threshold, proceed with DTW for better matching
        some_threshold = 20  # some_threshold is another hyperparameter
        #if max(lag) > some_threshold:
        # Step 4: Dynamic Time Warping
        distance, path = self.compute_dtw(fft_local, fft_remote)

        # Compute the average delay
        delay_diffs = [y - x for (x, y) in path]
        avg_delay_ms = np.mean(delay_diffs) * (1000 / self.fs)

        return avg_delay_ms

    def measure_peak_time_delay(self):
        # Step 1: Peak Detection
        peak_indices_local = self.find_peak_indices(self.audio_data_local)
        peak_indices_remote = self.find_peak_indices(self.audio_data_remote)

        # Step 2: FFT Spectrum
        fft_local = self.fft_spectrum(self.audio_data_local)
        fft_remote = self.fft_spectrum(self.audio_data_remote)

        # Step 3: Guided Correlation around Peaks
        lags = []
        for peak in peak_indices_local:
            window = 25  # This is a hyperparameter you'd need to tune

            # Check for boundary conditions
            if peak < window or peak + window >= len(fft_local):
                print("Skipping peak", peak, "due to boundary conditions.")
                continue

            sub_seq_local = fft_local[peak - window: peak + window]
            sub_seq_remote = fft_remote[peak - window: peak + window]

            if len(sub_seq_local) == 0 or len(sub_seq_remote) == 0:
                print("Skipping due to empty sub-sequence.")
                continue

            corr = correlate(sub_seq_local, sub_seq_remote, mode='full')
            lag = np.argmax(np.abs(corr)) - len(sub_seq_local) + 1
            lags.append(lag)

        if len(lags) == 0:
            print("No valid lags found.")
            return None

        # Calculate the average lag
        average_lag = mean(lags)
        
        # Convert lag to time delay in milliseconds
        time_delay_ms = (average_lag / self.fs) * 1000
        return time_delay_ms
    
    def get_average_delay3(self):
        try:
            #self.audio_data_local = self.low_pass_filter(self.audio_data_local)
            #self.audio_data_remote = self.low_pass_filter(self.audio_data_remote)

            # Cross-correlate the snippet with the entire second signal
            corr = correlate(self.audio_data_local, self.audio_data_remote, mode='valid')

            # Find the position of the maximum correlation
            max_corr_idx = np.argmax(corr)

            # Calculate the lag in samples
            lag_samples = max_corr_idx - (len(self.audio_data_remote) - 1)

            # Convert lag to time (in ms)
            #delay_ms = (lag_samples / self.fs) * 1000

            # Convert index to time (in ms)
            delay_ms = (max_corr_idx / self.fs) * 1000

            return delay_ms
        except Exception as e:
            print("An error occurred:", e)
            return None
        
    def get_average_delay2(self):
        try:
            self.audio_data_local = self.low_pass_filter(self.audio_data_local)
            self.audio_data_remote = self.low_pass_filter(self.audio_data_remote)

            peak_indices_local = self.find_peak_indices(self.audio_data_local)
            peak_indices_remote = self.find_peak_indices(self.audio_data_remote)

            if len(peak_indices_local) == 0 or len(peak_indices_remote) == 0:
                print("No peaks found.")
                return None

            fft_local = np.fft.fft(self.audio_data_local)
            fft_remote = np.fft.fft(self.audio_data_remote)

            lags = []
            for peak in peak_indices_local:
                window = 50  # Window size
                if peak < window or peak + window >= len(fft_local):
                    continue

                sub_seq_local = fft_local[peak - window: peak + window]

                if peak < window or peak + window >= len(fft_remote):
                    continue

                sub_seq_remote = fft_remote[peak - window: peak + window]
                corr = correlate(sub_seq_local, sub_seq_remote)
                lag = np.argmax(np.abs(corr)) - len(sub_seq_local) + 1
                lags.append(lag)

            if len(lags) == 0:
                print("No valid lags found.")
                return None

            actual_lag = np.mean(lags)
            delay_ms = (actual_lag / self.fs) * 1000

            return delay_ms

        except Exception as e:
            print("An error occurred:", e)
            return None