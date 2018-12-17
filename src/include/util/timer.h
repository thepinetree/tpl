#pragma once

#include <chrono>

namespace tpl::util {

/// A simple restartable timer
template <typename ResolutionRatio = std::milli>
class Timer {
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

 public:
  Timer() noexcept : elapsed_(0) { Start(); }

  void Start() noexcept { start_ = Clock::now(); }

  void Stop() noexcept {
    stop_ = Clock::now();

    elapsed_ =
        std::chrono::duration_cast<
            std::chrono::duration<double, ResolutionRatio>>(stop_ - start_)
            .count();
  }

  double elapsed() const noexcept { return elapsed_; }

 private:
  TimePoint start_;
  TimePoint stop_;

  double elapsed_;
};

/// An RAII timer that begins timing upon construction and stops timing when the
/// object goes out of scope. The total elapsed time is written to the output
/// \a elapsed argument.
template <typename ResolutionRatio = std::milli>
class ScopedTimer {
 public:
  explicit ScopedTimer(double *elapsed) noexcept : elapsed_(elapsed) {
    *elapsed_ = 0;
    timer_.Start();
  }

  ~ScopedTimer() {
    timer_.Stop();
    *elapsed_ = timer_.elapsed();
  }

 private:
  Timer<ResolutionRatio> timer_;
  double *elapsed_;
};

}  // namespace tpl::util