# Winner trophy artwork

Eight supplied 32x32 RGBA PNG frames and their original GIF timing define the results trophy. `python tools/compile_trophy_art.py` regenerates the indexed runtime header. The animation is drawn only on 1v1 and party results screens beside the winning player name. Simultaneous top-outs replay automatically until one winner remains. No rating result is recorded for an unfinished replay round. All three ports share this renderer.
