document.addEventListener("DOMContentLoaded", () => {
  const timerElement = document.getElementById("timer-message");
  let remainingTimeMs = 0;

  /**
   * Formats milliseconds into a MM:SS string.
   */
  function formatTime(ms) {
    if (ms <= 0) return "00:00";

    let totalSeconds = Math.ceil(ms / 1000);
    let minutes = Math.floor(totalSeconds / 60);
    let seconds = totalSeconds % 60;

    return `${minutes.toString().padStart(2, "0")}:${seconds
      .toString()
      .padStart(2, "0")}`;
  }

  /**
   * Polls the server to confirm the lockout is *truly* over
   * before redirecting. This fixes timer drift.
   */
  async function verifyCooldownFinished() {
    try {
      const response = await fetch("/cooldown-time");
      if (!response.ok) {
        throw new Error("Server error while verifying");
      }

      const serverTimeMs = parseInt(await response.text(), 10);

      if (serverTimeMs <= 0) {
        window.location.href = "/";
      } else {
        // Client timer was fast. Server is still locked.
        // Wait 300ms and check again.
        setTimeout(verifyCooldownFinished, 300);
      }
    } catch (error) {
      console.error("Error verifying cooldown:", error);
      // Retry after 3 seconds
      setTimeout(verifyCooldownFinished, 3000);
    }
  }

  /**
   * Fetches the initial time and handles the startup race condition.
   * Then starts the smooth client-side timer.
   */
  async function startTimer() {
    try {
      let response = await fetch("/cooldown-time");
      if (!response.ok) throw new Error("Failed to get initial time");

      remainingTimeMs = parseInt(await response.text(), 10);

      if (remainingTimeMs <= 0) {
        await new Promise((resolve) => setTimeout(resolve, 100)); // 100ms wait

        response = await fetch("/cooldown-time");
        if (!response.ok) throw new Error("Failed to get retry time");

        remainingTimeMs = parseInt(await response.text(), 10);
      }

      if (remainingTimeMs <= 0) {
        // It's *still* 0. We are genuinely not locked out. Redirect.
        window.location.href = "/";
        return;
      }

      timerElement.textContent = formatTime(remainingTimeMs);

      const timerInterval = setInterval(() => {
        remainingTimeMs -= 1000;

        if (remainingTimeMs <= 0) {
          clearInterval(timerInterval);
          timerElement.textContent = "00:00";

          // Timer finished, verify with the server to be sure
          verifyCooldownFinished();
        } else {
          // Update the visual timer
          timerElement.textContent = formatTime(remainingTimeMs);
        }
      }, 1000);
    } catch (error) {
      console.error("Error starting cooldown timer:", error);
      timerElement.textContent = "Error loading timer.";
      timerElement.className = "message-error";
    }
  }

  startTimer();
});
