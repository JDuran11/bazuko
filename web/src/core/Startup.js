export async function loadInBatches(items, load, onProgress = () => {}, concurrency = 4) {
  let next = 0;
  let completed = 0;
  await Promise.all(Array.from({ length: Math.min(concurrency, items.length) }, async () => {
    while (next < items.length) {
      await load(items[next++]);
      await onProgress(++completed, items.length);
    }
  }));
}

export async function withLoadTimeout(promise, message, milliseconds = 120000) {
  let timer;
  try {
    return await Promise.race([
      promise,
      new Promise((_, reject) => { timer = setTimeout(() => reject(new Error(message)), milliseconds); })
    ]);
  } finally { clearTimeout(timer); }
}
