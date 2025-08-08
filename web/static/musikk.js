document.addEventListener("fx:swapped", (evt)=>{
    if (evt.detail.cfg.target.id === "player-source") {
	const player = document.getElementById("audio-player");
	player.load();
	player.play();
    }
})

