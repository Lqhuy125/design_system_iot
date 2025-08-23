const overlay = document.getElementById('overlay');
document.getElementById('chartCard').addEventListener('click', () => {
  overlay.classList.add('active');
});
overlay.addEventListener('click', () => {
  overlay.classList.remove('active');
});
