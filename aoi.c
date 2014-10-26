#include <lua.h>
#include <lauxlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define OBJECTPOOL 64
#define DEFAULT_LEVEL 8
#define COMMON_LEVEL 0

struct dlink_node {
	struct dlink_node *pre_;
	struct dlink_node *next_;
};

struct dlink {
	struct dlink_node head_;
	struct dlink_node tail_;
};

struct point {
	float x;
	float y;
};

struct object {
	struct dlink_node node;
	struct point cur;
	struct point des;
	int id;
	int level;
	int range;
	struct object *next;
};

struct objectpool_list {
	struct objectpool_list *next;
	struct object pool[OBJECTPOOL];
};

struct objectpool {
	struct objectpool_list *pool;
	struct object *freelist;
};

struct tile {
	struct dlink **headers;
	int size;
	int x;
	int y;
};

struct map {
	int realwidth;
	int realhigh;
	int width;
	int high;
	int max_x_index;
	int max_y_index;
	int tile_len;
	int tile_sz;
	struct tile *tiles;
};

struct aoi_context {
	struct map map;
	struct objectpool *pool;
};

int 
dlink_remove(struct dlink_node *node);

int 
dlink_empty(struct dlink *dl) {
	return dl->head_.next_ == &dl->tail_ ? 1:0;
}

struct dlink_node *
dlink_pop(struct dlink *dl) {
	if (dlink_empty(dl))
		return 0;
	else {
		struct dlink_node *node = dl->head_.next_;
		dlink_remove(node);
		return node;
	}
}

int 
dlink_push(struct dlink *dl,struct dlink_node *node) {
	if (node->pre_ || node->next_)
		return -1;

	dl->tail_.pre_->next_ = node;
	node->pre_ = dl->tail_.pre_;
	dl->tail_.pre_ = node;
	node->next_ = &dl->tail_;
	return 0;
}

int 
dlink_remove(struct dlink_node *node) {
	if (!node->pre_ || !node->next_)
		return -1;
	node->pre_->next_ = node->next_;
	node->next_->pre_ = node->pre_;
	node->pre_ = node->next_ = 0;
	return 0;
}

void 
dlink_clear(struct dlink *dl) {
	dl->head_.pre_ = dl->tail_.next_ = 0;
	dl->head_.next_ = &dl->tail_;
	dl->tail_.pre_ = &dl->head_;
}

struct tile *tile_withrc(struct map *m,int r,int c);


void
tile_init(struct map *m) {
	m->tiles = malloc(m->tile_sz * sizeof(struct tile));
	int x, y;
	for (x = 0; x <= m->max_x_index; x++) {
		for (y = 0; y <= m->max_y_index; y++) {
			struct tile* tl = tile_withrc(m, y, x);
			tl->x = x;
			tl->y = y;
			tl->size = DEFAULT_LEVEL;
			tl->headers = malloc(tl->size * sizeof(struct dlink *));
			memset(tl->headers,0,tl->size * sizeof(struct dlink *));
		}
	}
}

struct tile*
tile_withrc(struct map *m,int r,int c) {
	if (c > m->max_x_index || r > m->max_y_index)
		return NULL;
	return &m->tiles[r * (m->max_x_index + 1) + c];
}

struct tile*
tile_withpos(struct map *m,struct point *pos) {
	int x = pos->x / m->tile_len;
	int y = pos->y / m->tile_len;
	if (x > m->max_x_index || y > m->max_y_index)
		return NULL;
	return tile_withrc(m,y,x);
}

struct dlink*
tile_level(struct tile* tl,int level) {
	if (level >= tl->size) {
		int nsize = level + 8;
		struct dlink **oheaders = tl->headers;
		int osize = tl->size;		
		tl->headers = malloc(nsize * sizeof(struct dlink*));
		memset(tl->headers,0,nsize * sizeof(struct dlink*));
		memcpy(tl->headers, oheaders, osize * sizeof(struct dlink*));
		free(oheaders);
		tl->size = nsize;
	}

	if (tl->headers[level] == NULL) {
		tl->headers[level] = malloc(sizeof(struct dlink));
		memset(tl->headers[level],0,sizeof(struct dlink));
		dlink_clear(tl->headers[level]);
	}		
	return tl->headers[level];
}

void 
tile_push(struct tile* tl,int level,struct dlink_node *node) {
	struct dlink *dl = tile_level(tl, level);
	dlink_push(dl, node);
}

void 
tile_pop(struct dlink_node *node) {
	dlink_remove(node);
}

float 
calc_dist(struct point *start, struct point *end) {
	float dx = start->x - end->x;
	dx *= dx;
	float dy = start->y - end->y;
	dy *= dy;
	return (float) sqrt(dx + dy);
}

int 
calc_rect(struct map *m, struct point *pos, int range, struct point *bl, struct point *tr) {
	struct tile *tl = tile_withpos(m, pos);
	if (tl == NULL)	
		return -1;

	bl->x = tl->x - range;
	bl->y = tl->y - range;
	tr->x = tl->x + range;
	tr->y = tl->y + range;

	if (bl->x < 0)
		bl->x = 0;
	if (bl->y < 0)
		bl->y = 0;
	if (tr->x > m->max_x_index)
		tr->x = m->max_x_index;
	if (tr->y > m->max_y_index)
		tr->y = m->max_y_index;

	return 0;
}

void 
make_table(lua_State *L,struct dlink *dl,struct object *self,int index,int flag) {
	int sindex = 1;
	int oindex = 1;

	struct object *obj = (struct object*) dl->head_.next_;
	while (obj != (struct object*) &dl->tail_) {
		if (obj == self) {
			obj = (struct object*) obj->node.next_;
			continue;
		}

		if (flag == 0) {
			lua_pushinteger(L, obj->id);
			lua_rawseti(L, index-2, sindex++);
		}
		lua_pushinteger(L, obj->id);
		lua_rawseti(L, index-1, oindex++);

		obj = (struct object*) obj->node.next_;
	}
}

int 
map_enter(lua_State *L,struct map *m,struct object *obj) {
	struct tile *tl = tile_withpos(m,&obj->cur);
	if (tl == NULL) {
		luaL_error(L,"[map_enter]invalid pos[%d:%d]",obj->cur.x,obj->cur.y);
		return -1;
	}

	struct point bl,tr;
	if (calc_rect(m,&obj->cur,obj->range,&bl,&tr) < 0) {
		luaL_error(L,"[map_enter]invalid pos[%d:%d],range[%d]",obj->cur.x,obj->cur.y,obj->range);
		return -1;
	}

	int x,y;
	for(y = bl.y;y <= tr.y;y++) {
		for(x = bl.x;x <= tr.x;x++) {
			struct tile *tl = tile_withrc(m,y,x);
			if (tl == NULL)
				return -1;

			struct dlink *dl = tile_level(tl, COMMON_LEVEL);
			make_table(L,dl,obj,-1,0);

			if (obj->level != COMMON_LEVEL) {
				dl = tile_level(tl, obj->level);
				make_table(L,dl,obj,-1,0);
			}
		}
	}
	tile_push(tl, obj->level, &obj->node);
	return 0;
}

int 
map_leave(lua_State *L,struct map *m,struct object *obj) {
	struct tile *tl = tile_withpos(m,&obj->cur);
	if (tl == NULL) {
		luaL_error(L,"[map_leave]invalid pos[%d:%d]",obj->cur.x,obj->cur.y);
		return -1;
	}

	struct point bl,tr;
	if (calc_rect(m,&obj->cur,obj->range,&bl,&tr) < 0) {
		luaL_error(L,"[map_leave]invalid pos[%d:%d],range[%d]",obj->cur.x,obj->cur.y,obj->range);
		return -1;
	}

	int x,y;
	for(y = bl.y;y <= tr.y;y++) {
		for(x = bl.x;x <= tr.x;x++) {
			struct tile *tl = tile_withrc(m,y,x);
			if (tl == NULL)
				return -1;

			struct dlink *dl = tile_level(tl, COMMON_LEVEL);
			make_table(L,dl,obj,-1,1);

			if (obj->level != COMMON_LEVEL) {
				dl = tile_level(tl, obj->level);
				make_table(L,dl,obj,-1,1);
			}
		}
	}

	tile_pop(&obj->node);
	return 0;
}

int 
map_update(lua_State *L,struct map *m,struct object *obj,struct point *np) {
	struct point op = obj->cur;
	obj->cur = *np;

	struct tile *otl = tile_withpos(m,&op);
	struct tile *ntl = tile_withpos(m,&obj->cur);
	if (otl == ntl)
		return 0;

	tile_pop(&obj->node);
	tile_push(ntl, obj->level, &obj->node);
	
	struct point obl, otr;
	if (calc_rect(m, &op, obj->range, &obl, &otr) < 0)
		return -1;

	struct point nbl, ntr;
	if (calc_rect(m, &obj->cur, obj->range, &nbl, &ntr) < 0)
		return -1;

	int x, y;
	for (y = nbl.y; y <= ntr.y; y++) {
		for (x = nbl.x; x <= ntr.x; x++) {
			if (x >= obl.x && x <= otr.x && y >= obl.y && y <= otr.y)
				continue;

			struct tile *tl = tile_withrc(m,y,x);
			if (tl == NULL)
				return -1;

			struct dlink *dl = tile_level(tl, COMMON_LEVEL);
			make_table(L,dl,obj,-3,0);

			if (obj->level != COMMON_LEVEL) {
				dl = tile_level(tl, obj->level);
				make_table(L,dl,obj,-3,0);
			}
		}
	}

	for (y = obl.y; y <= otr.y; y++) {
		for (x = obl.x; x <= otr.x; x++) {
			if (x >= nbl.x && x <= ntr.x && y >= nbl.y && y <= ntr.y)
				continue;

			struct tile *tl = tile_withrc(m,y,x);
			if (tl == NULL)
				return -1;

			struct dlink *dl = tile_level(tl, COMMON_LEVEL);
			make_table(L,dl,obj,-1,0);

			if (obj->level != COMMON_LEVEL) {
				dl = tile_level(tl, obj->level);
				make_table(L,dl,obj,-1,0);
			}
		}
	}
	return 0;
}

int 
_aoi_new(lua_State *L) {
	int realwidth = luaL_checkinteger(L, 1);
	int realhigh = luaL_checkinteger(L, 2);
	int tile_len = luaL_checkinteger(L, 3);

	int max_x_index = realwidth / tile_len - 1;
	int max_y_index = realhigh / tile_len - 1;

	int width = (max_x_index + 1) * tile_len;
	int high = (max_y_index + 1) * tile_len;

	struct aoi_context *aoictx = malloc(sizeof(*aoictx));
	memset(aoictx,0,sizeof(*aoictx));

	aoictx->pool = malloc(sizeof(*aoictx->pool));
	memset(aoictx->pool,0,sizeof(*aoictx->pool));

	aoictx->map.realwidth = realwidth;
	aoictx->map.realhigh = realhigh;
	aoictx->map.width = width;
	aoictx->map.high = high;
	aoictx->map.max_x_index = max_x_index;
	aoictx->map.max_y_index = max_y_index;
	aoictx->map.tile_len = tile_len;   //tile length
	aoictx->map.tile_sz = (max_x_index + 1) * (max_y_index + 1);   //amount of tiles in map
	
	tile_init(&aoictx->map);

	lua_pushlightuserdata(L, aoictx);
	return 1;
}

int 
_aoi_delete(lua_State *L) {
	struct aoi_context *aoi = lua_touserdata(L, 1);
	struct objectpool_list *p = aoi->pool->pool;
	while(p) {
		struct objectpool_list *tmp = p;
		p = p->next;
		free(tmp);
	}
	free(aoi->map.tiles);
	free(aoi);
	return 0;
}

int 
_aoi_enter(lua_State *L) {
	struct aoi_context *aoi = lua_touserdata(L, 1);
	int id = luaL_checkinteger(L, 2);
	float curx = luaL_checknumber(L, 3);
	float cury = luaL_checknumber(L, 4);
	float desx = luaL_checknumber(L, 5);
	float desy = luaL_checknumber(L, 6);
	int level = luaL_checkinteger(L,7);
	int range = luaL_checkinteger(L,8);

	if (curx < 0 || cury < 0 || curx >= aoi->map.width || cury >= aoi->map.high) {
		luaL_error(L,"[_aoi_enter]invalid cur pos[%d:%d]",curx,cury);
		return 0;
	}
	if (desx < 0 || desy < 0 || desx >= aoi->map.width || desy >= aoi->map.high) {
		luaL_error(L,"[_aoi_enter]invalid des pos[%d:%d]",desx,desy);
		return 0;
	}

	struct object *obj;
	if (aoi->pool->freelist) {
		obj = aoi->pool->freelist;
		aoi->pool->freelist = obj->next;
	} else {
		struct objectpool_list *opl = malloc(sizeof(*opl));
		struct object * temp = opl->pool;
		memset(temp,0,sizeof(struct object) * OBJECTPOOL);
		int i;
		for (i=1;i<OBJECTPOOL;i++) {
			temp[i].next = &temp[i+1];
		}
		temp[OBJECTPOOL-1].next = NULL;
		opl->next = aoi->pool->pool;
		aoi->pool->pool = opl;
		obj = &temp[0];
		aoi->pool->freelist = &temp[1];
	}
	
	obj->id = id;
	obj->cur.x = curx;
	obj->cur.y = cury;
	obj->des.x = desx;
	obj->des.y = desy;
	obj->level = level;
	obj->range = range;

	lua_pushlightuserdata(L, obj);
	lua_newtable(L); //-2 enter self
	lua_newtable(L); //-1 enter other

	map_enter(L,&aoi->map,obj);
	return 3;
}

int
_aoi_leave(lua_State *L) {
	struct aoi_context *aoi = lua_touserdata(L, 1);
	struct object *obj = lua_touserdata(L, 2);

	lua_newtable(L); //-1 leave other
	map_leave(L,&aoi->map,obj);

	obj->next = aoi->pool->freelist;
	aoi->pool->freelist = obj;

	return 1;
}

int 
_aoi_update(lua_State *L) {
	struct aoi_context *aoi = lua_touserdata(L, 1);
	struct object *obj = lua_touserdata(L, 2);

	struct point op;
	op.x = luaL_checknumber(L, 3);
	op.y = luaL_checknumber(L, 4);

	struct point np;
	np.x = luaL_checknumber(L, 5);
	np.y = luaL_checknumber(L, 6);

	if (op.x >= aoi->map.width || op.y >= aoi->map.high || np.x >= aoi->map.width || np.y >= aoi->map.high) {
		luaL_error(L,"[_aoi_update]invalid pos[%d:%d]->[%d:%d].",op.x,op.y,np.x,np.y);
		return 0; 
	}

	if (op.x < 0 || op.y < 0 || np.x < 0 || np.y < 0) {
		luaL_error(L,"[_aoi_update]invalid pos[%d:%d]->[%d:%d].",op.x,op.y,np.x,np.y);
		return 0; 
	}

	lua_newtable(L); //-4 enter self
	lua_newtable(L); //-3 enter other
	lua_newtable(L); //-2 leave self
	lua_newtable(L); //-1 leave other

	int ret = 0;
	if (calc_dist(&obj->cur, &op) != 0) {
		if ((ret = map_update(L,&aoi->map, obj, &op)) < 0) {
			luaL_error(L,"[_aoi_update]erro:%d",ret);
			return 0; 
		}
	}

	if ((ret = map_update(L,&aoi->map, obj, &np)) < 0) {
		luaL_error(L,"[_aoi_update]erro:%d.",ret);
		return 0; 
	}
	return 4;
}

int 
_aoi_viewlist(lua_State *L) {
	struct aoi_context *aoi = lua_touserdata(L, 1);
	struct object *obj = lua_touserdata(L, 2);

	struct tile *tl = tile_withpos(&aoi->map,&obj->cur);
	if (tl == NULL) {
		luaL_error(L,"[_aoi_viewlist]invalid pos[%d:%d]",obj->cur.x,obj->cur.y);
		return 0;
	}

	struct point bl,tr;
	if (calc_rect(&aoi->map,&obj->cur,obj->range,&bl,&tr) < 0) {
		luaL_error(L,"[_aoi_viewlist]invalid pos[%d:%d],range[%d]",obj->cur.x,obj->cur.y,obj->range);
		return 0;
	}

	lua_newtable(L);

	int x,y;
	for(y = bl.y;y <= tr.y;y++) {
		for(x = bl.x;x <= tr.x;x++) {
			struct tile *tl = tile_withrc(&aoi->map,y,x);
			if (tl == NULL) {
				luaL_error(L,"[_aoi_viewlist]invalid rc[%d:%d],range[%d]",y,x);
				return 0;
			}

			struct dlink *dl = tile_level(tl, COMMON_LEVEL);
			make_table(L,dl,obj,-1,1);

			if (obj->level != COMMON_LEVEL) {
				dl = tile_level(tl, obj->level);
				make_table(L,dl,obj,-1,1);
			}
		}
	}

	return 1;
}

int 
luaopen_aoi_c(lua_State *L)
{
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "aoi_new", _aoi_new},
		{ "aoi_delete", _aoi_delete},
		{ "aoi_enter", _aoi_enter},
		{ "aoi_leave", _aoi_leave},
		{ "aoi_update", _aoi_update},
		{ "aoi_viewlist", _aoi_viewlist},
		{ NULL, NULL },
	};

	lua_createtable(L, 0, (sizeof(l)) / sizeof(luaL_Reg) - 1);
	luaL_setfuncs(L, l, 0);
	return 1;
}

