
#include "pck.h"

PCK::PCK( std::string PckFilename, std::string TabFilename, Palette* ColourPalette )
{
	Colours = ColourPalette;

	ALLEGRO_FILE* pck = al_fopen( PckFilename.c_str(), "rb" );
	ALLEGRO_FILE* tab = al_fopen( TabFilename.c_str(), "rb" );

	int16_t version = al_fread16le( pck );
	al_fseek( pck, 0, ALLEGRO_SEEK_SET );
	switch( version )
	{
		case 0:
			LoadVersion1Format( pck, tab );
			break;
		case 1:
			LoadVersion2Format( pck, tab );
			break;
	}

	al_fclose( tab );
	al_fclose( pck );
}

PCK::~PCK()
{
}

int PCK::GetImageCount()
{
	return images.size();
}

void PCK::RenderImage( int Index, int X, int Y )
{
	al_draw_bitmap( images.at(Index), X, Y, 0 );
}

void PCK::LoadVersion1Format(ALLEGRO_FILE* pck, ALLEGRO_FILE* tab)
{
	ALLEGRO_BITMAP* bitmap;
	ALLEGRO_LOCKED_REGION* region;

	int16_t c0_offset;
	int16_t c0_maxwidth;
	int16_t c0_height;
	Memory* c0_imagedata;
	size_t c0_bufferptr;
	int c0_idx;
	std::vector<int16_t> c0_rowwidths;

	for( int i = 0; i < al_fsize( tab ) / 4; i++ )
	{
		unsigned int offset = al_fread32le( tab );

		al_fseek( pck, offset, ALLEGRO_SEEK_SET );

		// Raw Data
		c0_offset = al_fread16le( pck );
		c0_imagedata = new Memory(0);
		c0_rowwidths.clear();
		c0_maxwidth = 0;
		c0_height = 0;

		while( c0_offset != -1 )
		{
			int16_t c0_width = al_fread16le( pck );	// I hope they never change width mid-image
			c0_rowwidths.push_back( c0_width );
			if( c0_maxwidth < c0_width )
			{
				c0_maxwidth = c0_width;
			}

			c0_bufferptr = c0_imagedata->GetSize();
			c0_imagedata->Resize( c0_bufferptr + c0_width + (c0_offset % 640) );
			memset( c0_imagedata->GetDataOffset( c0_bufferptr ), 0, c0_width + (c0_offset % 640) );
			al_fread( pck, c0_imagedata->GetDataOffset( c0_bufferptr + (c0_offset % 640) ), c0_width );
			c0_height++;

			c0_offset = al_fread16le( pck );	// Always a 640px row (that I've seen)
		}
		bitmap = al_create_bitmap( c0_maxwidth, c0_height );
		region = al_lock_bitmap( bitmap, ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE, 0 );
		c0_idx = 0;
		for( int c0_y = 0; c0_y < c0_height; c0_y++ )
		{
			for( int c0_x = 0; c0_x < c0_maxwidth; c0_x++ )
			{
				int dataptr = (c0_y * region->pitch) + (c0_x * 4);
				Colour* c0_rowptr = (Colour*)(&((char*)region->data)[ dataptr ]);
				Colour* c0_palcol;
				if( c0_x < c0_rowwidths.at( c0_y ) )
				{
					c0_palcol = Colours->GetColour( ((char*)c0_imagedata->GetDataOffset( c0_idx ))[0] );
				} else {
					c0_palcol = Colours->GetColour( 0 );
				}
				c0_rowptr->a = c0_palcol->a;
				c0_rowptr->r = c0_palcol->r;
				c0_rowptr->g = c0_palcol->g;
				c0_rowptr->b = c0_palcol->b;

				c0_idx++;
			}
		}
		al_unlock_bitmap( bitmap );
		images.push_back( bitmap );
		delete c0_imagedata;

	}
}

void PCK::LoadVersion2Format(ALLEGRO_FILE* pck, ALLEGRO_FILE* tab)
{
	int16_t compressionmethod;
	Memory* tmp;

	ALLEGRO_BITMAP* bitmap;
	ALLEGRO_LOCKED_REGION* region;

	PCKCompression1ImageHeader c1_imgheader;
	int32_t c1_pixelstoskip;
	PCKCompression1RowHeader c1_header;


	for( int i = 0; i < al_fsize( tab ) / 4; i++ )
	{
		unsigned int offset = al_fread32le( tab ) * 4;

		al_fseek( pck, offset, ALLEGRO_SEEK_SET );

		compressionmethod = al_fread16le( pck );
		switch( compressionmethod )
		{
			case 0:

				break;

			case 1:
				// Raw Data with RLE
				al_fread( pck, &c1_imgheader, sizeof( PCKCompression1ImageHeader ) );
				bitmap = al_create_bitmap( c1_imgheader.RightMostPixel - c1_imgheader.LeftMostPixel, c1_imgheader.BottomMostPixel - c1_imgheader.TopMostPixel );
				region = al_lock_bitmap( bitmap, ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE, 0 );

				c1_pixelstoskip = al_fread32le( pck );
				while( c1_pixelstoskip != -1 )
				{
					al_fread( pck, &c1_header, sizeof( PCKCompression1Header ) );
					int c1_y = (c1_pixelstoskip / 640) - c1_imgheader.TopMostPixel;

					for( int c1_x = 0; c1_x < c1_header.PixelsInRow - c1_header.PaddingInRow; c1_x++ )
					{
						if( (c1_header.ColumnToStartAt + c1_x - c1_imgheader.LeftMostPixel) < c1_imgheader.RightMostPixel - c1_imgheader.LeftMostPixel )
						{
							int dataptr = (c1_y * region->pitch) + ((c1_header.ColumnToStartAt + c1_x - c1_imgheader.LeftMostPixel) * 4);

							Colour* c1_rowptr = (Colour*)(&((char*)region->data)[ dataptr ]);
							Colour* c1_palcol = Colours->GetColour( al_fgetc( pck ) );
							c1_rowptr->a = c1_palcol->a;
							c1_rowptr->r = c1_palcol->r;
							c1_rowptr->g = c1_palcol->g;
							c1_rowptr->b = c1_palcol->b;
						} else {
							// Pretend to process data
							al_fgetc( pck );
						}
					}
					tmp = new Memory(c1_header.PaddingInRow);
					al_fread( pck, tmp->GetData(), c1_header.PaddingInRow );
					delete tmp;

					c1_pixelstoskip = al_fread32le( pck );
				}
				al_unlock_bitmap( bitmap );
				images.push_back( bitmap );
				break;

			case 2:
				// Divide by 0 should indicate we've tried to load a Mode 2 compressed image
				compressionmethod = 0;
				compressionmethod = 2 / compressionmethod;
				break;

			case 3:
				// Divide by 0 should indicate we've tried to load a Mode 3 compressed image
				compressionmethod = 0;
				compressionmethod = 2 / compressionmethod;
				break;

			case 128:
				// Divide by 0 should indicate we've tried to load a Mode 128 compressed image
				compressionmethod = 0;
				compressionmethod = 2 / compressionmethod;
				break;

			default:
				// No idea
				compressionmethod = 0;
				compressionmethod = 2 / compressionmethod;
				break;
		}

	}
}
