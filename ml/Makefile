include ../common/make.inc

SOURCETILES:=../comp2/tiles-${RESX}x${RESY}x${BLOCKSIZE}.dat
STREAM:=../comp2/stream-${RESX}x${RESY}x${BLOCKSIZE}.dat

setup :
	cp $(SOURCETILES) kmeans_ni_mse.dat
	cp $(SOURCETILES) stream-kmeans_256_ni.dat
	cp $(SOURCETILES) kmeans_256_ni_mse.dat

	cp ../comp2/badapple-sm8628149.mp4 "Touhou - Bad Apple.mp4"
	echo "img_size = (${RESY}, ${RESX})" > blocksettings.py
	echo "block_size = (${BLOCKSIZE}, ${BLOCKSIZE})" >> blocksettings.py
	echo "nblocks = ${TARGET_GLYPH_COUNT}" >> blocksettings.py

runcomp :  
	python3 train_patches.py

## Then once you're happy...

MOST_RECENT_RUN:=$(lastword $(shell ls outputs/))

MOST_RECENT_STREAM:=$(lastword $(shell ls outputs/$(MOST_RECENT_RUN)/data/*stream.dat))
MOST_RECENT_BLOCKS:=$(lastword $(shell ls outputs/$(MOST_RECENT_RUN)/data/*blocks.dat))
OUTFOLDER:=$(basename $(MOST_RECENT_RUN_BLOCK))_$(MOST_RECENT_RUN)

pullstream :
	cp $(MOST_RECENT_STREAM) $(STREAM)
	cp $(MOST_RECENT_BLOCKS) $(SOURCETILES)
	


MOST_RECENT_RUN_BLOCK:=$(lastword $(shell ls outputs/$(MOST_RECENT_RUN)/blocks/))
OUTFOLDER:=$(basename $(MOST_RECENT_RUN_BLOCK))_$(MOST_RECENT_RUN)

reconstruct :
	python3 reconstruct_video_blocks.py outputs/$(MOST_RECENT_RUN)/blocks/$(MOST_RECENT_RUN_BLOCK)
	ls -l $(OUTFOLDER)/sequence.dat $(OUTFOLDER)/tilemap.dat
	cp $(OUTFOLDER)/tilemap.dat $(SOURCETILES)
	cp $(OUTFOLDER)/sequence.dat $(STREAM)

squeeze :
	cp $(SOURCETILES) kmeans_256_ni_mse.dat
	cp ../streamrecomp/stream_stripped.dat stream-kmeans_256_ni.dat
	python3 train_patches_sequence.py

MOST_RECENT_STREAMOUT:=$(lastword $(shell ls outputs/$(MOST_RECENT_RUN)/data/*stream.dat))
MOST_RECENT_BLOCKSOUT:=$(lastword $(shell ls outputs/$(MOST_RECENT_RUN)/data/*blocks.dat))

squeezepull :
	cp $(MOST_RECENT_STREAMOUT) $(STREAM)
	cp $(MOST_RECENT_STREAMOUT) ../streamrecomp/stream_stripped.dat
	cp $(MOST_RECENT_BLOCKSOUT) $(SOURCETILES)


