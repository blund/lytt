document.addEventListener("fx:swapped", (evt)=>{
    if (evt.detail.cfg.target.id === "player-source") {

	const player = document.getElementById("audio-player");
	const source = document.getElementById("player-source");

	document.getElementById('track-title').textContent = source.dataset.title;
	document.getElementById('track-artist').textContent = source.dataset.artist;
	document.getElementById('track-cover').src = source.dataset.cover;

	player.load();
	player.play();
    }
})
