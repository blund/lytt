
document.addEventListener("fx:swapped", (evt)=>{
    if (evt.detail.cfg.target.id === "player-source") {

	const player = document.getElementById("audio-player");
	const source = document.getElementById("player-source");

	document.getElementById('track-title').textContent = source.dataset.title;
	document.getElementById('track-artist').textContent = source.dataset.artist;
	document.getElementById('track-cover').src = source.dataset.cover;

	if ("mediaSession" in navigator) {
	    console.log("mediaSession")
	    navigator.mediaSession.metadata = new MediaMetadata({
		title: source.dataset.title,
		artist: source.dataset.arist,
		artwork: [
		    {
			src: source.dataset.cover,
			sizes: "300x300",
			type: "img/jpeg",
		    }
		]
	    });
	}

	player.onended = (event) => {
	    console.log("Song finished!");
	    const source = document.getElementById("player-source");
	    source.dispatchEvent(new Event('nextSong', { bubbles: true }));
	}

	player.load();
	player.play();


    }
})
