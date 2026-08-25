/**
 * Ozayn Service Worker - DISABLED
 * Do not register this service worker.
 * It was causing stale cache issues in the desktop app.
 */

// Immediately unregister any old service worker
self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', () => {
    self.registration.unregister();
    self.clients.matchAll().then(clients => {
        clients.forEach(client => client.navigate(client.url));
    });
});
