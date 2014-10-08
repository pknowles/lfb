


int iceil(int n, int d)
{
	return n / d + (n % d > 0 ? 1 : 0);
}

int tilesIndex(ivec2 vpSize, ivec2 tileSize, ivec2 pixelCoord)
{
	#if 0
	//passthrough
	return pixelCoord.y * vpSize.x + pixelCoord.x;
	#endif	
	
	//standard matrix within tile
	#if 1
	ivec2 indexBase = pixelCoord / tileSize;
	ivec2 indexTile = pixelCoord - indexBase * tileSize; 
	return tileSize.y * (indexBase.y * vpSize.x + indexBase.x * tileSize.x) + indexTile.y * tileSize.x + indexTile.x;
	#endif
	
	/*
	//procedural kepler within tile
	#if 1
	ivec2 base = pixelCoord / ivec2(2, 8);
	ivec2 tile = pixelCoord % ivec2(2, 8);
	return base.y * lfbInfolfb.size.x * 8 + base.x * 8 * 2 + tile.y * 2 + tile.x;
	#endif
	*/
}

ivec2 reverseTilesIndex(ivec2 vpSize, ivec2 tileSize, int width, int index)
{
	//account for padding at the end of each row
	index += (vpSize.x - width) * index / width;
	
	//standard matrix within tile
	#if 1
	int tilesx = vpSize.x / tileSize.x;
	int n = tileSize.x * tileSize.y;
	int tile = index / n;
	int tileIndex = index % n;
	return ivec2(tile % tilesx, tile / tilesx) * tileSize + ivec2(tileIndex % tileSize.x, tileIndex / tileSize.x);
	#endif
}
