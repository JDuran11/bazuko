import '@fontsource/overpass/600.css';
import '@fontsource/overpass/800.css';
import '@fontsource/overpass/900.css';
import { LoadingScreen } from './ui/LoadingScreen.js';

const loading = new LoadingScreen();
try {
  await loading.stage('Preparando el juego…', 0);
  const { Game } = await import('./core/game.js');
  const game = new Game(document.getElementById('game-canvas'));
  await game.start(loading);
} catch (error) { loading.error(error); }
