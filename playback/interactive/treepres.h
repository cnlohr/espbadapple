#ifndef _TREEPRES_H
#define _TREEPRES_H

struct treepresnode
{
	struct treepresnode * parent;
	struct treepresnode * child[2];
	char label[32];
	int value;
	float x, y, z;
	float fx, fy, fz; //Force (For spring calculation phase)
};

void PerformTreeRelax( struct treepresnode * tree, int nodes )
{
	int springs[nodes*2][2]; // Cannot be more springs than nodes*2.
	int springct = 0;
	const float idealspringsize = 38;
	const float springforce_base = 0.10;
	float repelforce;
	float springforce = springforce_base;
	int i;
	for( i = 0; i < nodes; i++ )
	{
		struct treepresnode * t = &tree[i];
		struct treepresnode * l = tree[i].child[0];
		struct treepresnode * r = tree[i].child[1];
		if( l )
		{
			springs[springct][0] = i;
			springs[springct][1] = l - tree;
			springct++;
		}
		if( r )
		{
			springs[springct][0] = i;
			springs[springct][1] = r - tree;
			springct++;
		}
	}

	printf( "Nodes: %d / %d\n", nodes, springct );
	for( i = 0; i < springct; i++ )
	{
		printf( "%d %d - %d\n", i, springs[i][0], springs[i][1] );
	}
	const int iterations = 20000;
	for( i = 0; i < iterations; i++ )
	{
		float yougness = 1.0 - (i+1) / (double)iterations;

		repelforce = 4200.8;

		// Perform mass spring system iterations.
		// Step 1. Perform springs first.
		int j;
		for( j = 0; j < nodes; j++ )
		{
			tree[j].fx *= 0.9;
			tree[j].fy *= 0.9;
			tree[j].fz *= 0.9;
		}

		for( j = 0; j < springct; j++ )
		{
			struct treepresnode * n1 = &tree[springs[j][0]];
			struct treepresnode * n2 = &tree[springs[j][1]];

			float dfx = n2->x - n1->x;
			float dfy = n2->y - n1->y;
			float dfz = n2->z - n1->z;
			float dist = sqrtf( dfx*dfx + dfy*dfy + dfz*dfz );
			//Normalize dfx,dfy
			dfx /= dist;
			dfy /= dist;
			dfz /= dist;
			float tdist = dist - idealspringsize;
			dfx *= tdist * springforce;
			dfy *= tdist * springforce;
			dfz *= tdist * springforce;

			n1->fx += dfx;
			n1->fy += dfy;
			n1->fz += dfz;
			n2->fx -= dfx;
			n2->fy -= dfy;
			n2->fz -= dfz;
		}

		for( j = 0; j < nodes; j++ )
		{
			struct treepresnode * n1 = &tree[j];
			int k;
			for( k = 0; k < nodes; k++ )
			{
				if( k == j ) continue;
				struct treepresnode * n2 = &tree[k];
				float dfx = n2->x - n1->x;
				float dfy = n2->y - n1->y;
				float dfz = n2->z - n1->z;
				float dist = sqrtf( dfx*dfx + dfy*dfy + dfz*dfz );

				// Apply repeling force
				dfx /= dist*dist*dist*dist;
				dfy /= dist*dist*dist*dist;
				dfz /= dist*dist*dist*dist;

				dfx *= -repelforce;
				dfy *= -repelforce;
				dfz *= -repelforce;

				n1->fx += dfx;
				n1->fy += dfy;
				n1->fz += dfz;
				n2->fx -= dfx;
				n2->fy -= dfy;
				n2->fz -= dfz;
			}
		}

		for( j = 1; j < nodes; j++ )
		{
			struct treepresnode * t = &tree[j];
			t->x += t->fx;
			t->y += t->fy;
			t->z += t->fz * yougness;
			t->z *= yougness;
		}
	}
}

struct treepresnode * GenTreeFromTable( uint16_t * table, int size, int * nnodes )
{
	srand( 0 );
	// Store terminal nodes at the end of the tree.
	struct treepresnode * tree = (struct treepresnode*)calloc( sizeof( struct treepresnode ), size*3 );
	*nnodes = size;
	int i;
	for( i = 0; i < size; i++ )
	{
		uint16_t tv = table[i];
		uint8_t left = tv>>8;
		uint8_t right = tv & 0xff;
		printf( "%d / %d / %d\n", i, left, right );
		struct treepresnode * t = &tree[i];
		if( !(left & 0x80) )
		{
			t->child[0] = &tree[left+i+1];
			tree[left+i+1].parent = t;
		}
		else
		{
			int lnode = ((*nnodes)++);
			t->child[0] = &tree[lnode];
			struct treepresnode * n = &tree[lnode];
			sprintf( n->label, "%02x", left );
			n->value = left;

			float pax = 0, pay = 0, paz = 0;
			if( t->parent ) { pax = t->parent->x; pay = t->parent->y; paz = t->parent->z; }
			float dirx = t->x - pax*.5;
			float diry = t->y - pay*.5;
			float dirz = t->z - paz*.5;
			if( sqrtf( dirx*dirx + diry*diry + dirz*dirz ) > 1.0 )
			{
				dirx /= sqrtf( dirx*dirx + diry*diry + dirz*dirz );
				diry /= sqrtf( dirx*dirx + diry*diry + dirz*dirz );
			}
			n->x = t->x + (rand()%101)-50 + dirx * 50;
			n->y = t->y + (rand()%101)-50 + diry * 50;
			n->z = t->z + (rand()%101)-50 + dirz * 50;

			n->parent = t;
		}

		if( !(right & 0x80) )
		{
			t->child[1] = &tree[right+i+1];
			tree[right+i+1].parent = t;
		}
		else
		{
			int lnode = ((*nnodes)++);
			t->child[1] = &tree[lnode];
			struct treepresnode * n = &tree[lnode];
			sprintf( n->label, "%02x", right&0x7f );
			n->value = right;

			float pax = 0, pay = 0, paz = 0;
			if( t->parent ) { pax = t->parent->x; pay = t->parent->y; paz = t->parent->z; }

			float dirx = t->x - pax*.5;
			float diry = t->y - pay*.5;
			float dirz = t->z - paz*.5;
			if( sqrtf( dirx*dirx + diry*diry + dirz*dirz ) > 1.0 )
			{
				dirx /= sqrtf( dirx*dirx + diry*diry + dirz*dirz );
				diry /= sqrtf( dirx*dirx + diry*diry + dirz*dirz );
			}

			n->x = t->x + (rand()%101)-50 + dirx * 50;
			n->y = t->y + (rand()%101)-50 + diry * 50;
			n->z = t->z + (rand()%101)-50 + dirz * 50;
			n->parent = t;
		}

		if( i == 0 )
		{
			t->x = 0;
			t->y = 0;
		}
		else
		{
			t->x = (rand()%101)-50;
			t->y = (rand()%101)-50;

			if( t->parent )
			{
				t->x += t->parent->x;
				t->y += t->parent->y;

				float dirx = t->parent->x;
				float diry = t->parent->y;
				float dirz = t->parent->z;
				if( sqrtf( dirx*dirx + diry*diry + dirz*dirz ) > 1.0 )
				{
					dirx /= sqrtf( dirx*dirx + diry*diry + dirz*dirz );
					diry /= sqrtf( dirx*dirx + diry*diry + dirz*dirz );
				}

				t->x += dirx * 50;
				t->y += diry * 50;
				t->z += dirz * 50;
			}
		}
	}
	PerformTreeRelax( tree, *nnodes );
	return tree;
}

#endif



