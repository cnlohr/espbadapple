
SOURCETILES:=../comp2/tiles-64x48x8.dat
STREAM:=../comp2/tiles-64x48x8.dat

setup :
	cp $(SOURCETILES) kmeans_ni_mse.dat
	cp ../comp2/badapple-sm8628149.mp4 "Touhou - Bad Apple.mp4"

runcomp :
	python3 train_patches.py

## Then once you're happy...

MOST_RECENT_RUN:=$(lastword $(shell ls outputs/))
MOST_RECENT_RUN_BLOCK:=$(lastword $(shell ls outputs/$(MOST_RECENT_RUN)/blocks/))
OUTFOLDER:=$(basename $(MOST_RECENT_RUN_BLOCK))_$(MOST_RECENT_RUN)

reconstruct :
	python3 reconstruct_video_blocks.py outputs/$(MOST_RECENT_RUN)/blocks/$(MOST_RECENT_RUN_BLOCK)
	ls -l $(OUTFOLDER)/sequence.dat $(OUTFOLDER)/tilemap.dat
	cp $(OUTFOLDER)/tilemap.dat $(SOURCETILES)
	cp $(OUTFOLDER)/sequence.dat $(STREAM)
