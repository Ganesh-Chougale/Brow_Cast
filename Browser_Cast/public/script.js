// public/script.js
// Optional: Add a simple way to retry the stream if it breaks
document.addEventListener('DOMContentLoaded', () => {
    const img = document.getElementById('remoteScreen');
    img.onerror = function() {
        console.log('Stream error, attempting to reload...');
        // Reload the image after a short delay
        setTimeout(() => {
            img.src = '/stream?' + new Date().getTime(); // Add cache busting
        }, 2000); // 2 second delay before retry
    };
});