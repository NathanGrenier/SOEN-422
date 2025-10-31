document.addEventListener("DOMContentLoaded", () => {
  const loginForm = document.getElementById("login-form");
  const messageEl = document.getElementById("message");

  if (loginForm) {
    loginForm.addEventListener("submit", async (event) => {
      event.preventDefault();

      const formData = new FormData(loginForm);
      const data = new URLSearchParams(formData);

      messageEl.textContent = "";
      messageEl.className = "";

      try {
        const response = await fetch("/login", {
          method: "POST",
          headers: {
            "Content-Type": "application/x-www-form-urlencoded",
          },
          body: data.toString(),
        });

        const responseText = await response.text();

        if (response.ok) {
          messageEl.textContent = responseText;
          messageEl.className = "message-success";
          loginForm.reset();
        } else {
          if (response.status === 403) {
            // Status 403 means we are locked out. Redirect to the cooldown page.
            window.location.href = "/cooldown.html";
          } else {
            // Otherwise, just show the error message
            messageEl.textContent = responseText;
            messageEl.className = "message-error";
          }
        }
      } catch (error) {
        console.error("Login failed:", error);
        messageEl.textContent = "Connection error. Please try again.";
        messageEl.className = "message-error";
      }
    });
  }
});
