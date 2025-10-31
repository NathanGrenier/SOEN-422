document.addEventListener("DOMContentLoaded", () => {
  const adminForm = document.getElementById("admin-form");
  const messageEl = document.getElementById("message");

  if (adminForm) {
    adminForm.addEventListener("submit", async (event) => {
      event.preventDefault();

      const formData = new FormData(adminForm);
      const data = new URLSearchParams(formData);

      messageEl.textContent = "";
      messageEl.className = "";

      try {
        const response = await fetch("/update-credentials", {
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
          adminForm.reset();
        } else {
          messageEl.textContent = responseText;
          messageEl.className = "message-error";
        }
      } catch (error) {
        console.error("Update failed:", error);
        messageEl.textContent = "Connection error. Please try again.";
        messageEl.className = "message-error";
      }
    });
  }
});
