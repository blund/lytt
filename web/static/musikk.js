
setControls();

document.addEventListener("fx:swapped", (evt)=>{
    if (evt.detail.cfg.target.id === "player-source") {

	const player = document.getElementById("audio-player");
	const source = document.getElementById("player-source");

	document.getElementById('track-title').textContent = source.dataset.title;
	document.getElementById('track-artist').textContent = source.dataset.artist;
	document.getElementById('track-cover').src = source.dataset.cover;

	player.onloadedmetadata = (event) => {
	    setMetadata(source, source.dataset.title, source.dataset.artist, source.dataset.cover);
	}

	player.onended = (event) => {
	    const source = document.getElementById("player-source");
	    playNext(source);
	    navigator.mediaSession.playbackState = "playing";
	}

	player.onpause = (event) => {
	    console.log("Song paused!");
	    navigator.mediaSession.playbackState = "paused";
	}

	player.load();
	player.play();

	navigator.mediaSession.playbackState = "playing";

	setControls();
    }
})

// For the 'next' and 'prev' media control buttons, read the direction set in the dataset
document.addEventListener("fx:before", evt => {
    if (evt.target.id === "player-source") {
	const source = document.getElementById("player-source");
	const dir = source.dataset.dir;

	// No direction, play the selected song
	if (!dir) return;

	// Has direction, append the 'dir' query parameter
	evt.detail.cfg.action += `&dir=${dir}`
    }
});

function setControls() {
    const player = document.getElementById("audio-player");
    const source = document.getElementById("player-source");

    if ("mediaSession" in navigator) {
	navigator.mediaSession.setActionHandler('nexttrack', () => {
	    playNext(source);
	});
	navigator.mediaSession.setActionHandler('previoustrack', () => {
	    playPrev(source);
	});
	navigator.mediaSession.setActionHandler('play', () => {
	    player.play();
	});
	navigator.mediaSession.setActionHandler('pause', () => {
	    player.pause();
	});
    }
}

function playNext(source) {
    source.dataset.dir = "next";
    source.dispatchEvent(new Event('play', { bubbles: true }));
}

function playPrev(source) {
    source.dataset.dir = "prev";
    source.dispatchEvent(new Event('play', { bubbles: true }));
}

function setMetadata(source, artist, title, cover) {
    if ("mediaSession" in navigator) {
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
	navigator.mediaSession.playbackState = "playing";
    }
}
