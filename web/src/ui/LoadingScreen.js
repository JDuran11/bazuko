export const paintLoadingFrame = () => new Promise(resolve => {
  requestAnimationFrame(() => requestAnimationFrame(resolve));
});

export class LoadingScreen {
  constructor() {
    this.root = document.getElementById('loading-screen');
    this.status = document.getElementById('loading-status');
    this.progress = document.getElementById('loading-progress');
    document.getElementById('loading-retry').onclick = () => location.reload();
  }

  async stage(text, progress) {
    if (this.failed) return;
    this.root.hidden = false;
    this.status.textContent = text;
    this.progress.value = progress;
    await paintLoadingFrame();
  }

  forCreator() { this.root.hidden = true; }

  finish() {
    this.root.hidden = true;
    document.body.classList.remove('is-loading');
  }

  error(error) {
    this.failed = true;
    console.error('[Inicio]', error);
    this.root.hidden = false;
    this.status.textContent = 'No se pudo completar la carga. Revisa la conexión y vuelve a intentarlo.';
    this.progress.hidden = true;
    document.getElementById('loading-retry').hidden = false;
  }
}
