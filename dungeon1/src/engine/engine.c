#define SUBKEY_TYPES                                                           \
	X(Position)                                                                \
	X(Rotation)                                                                \
	X(Color)                                                                   \
	X(Camera)                                                                  \
	X(Model)                                                                   \
	X(Material)                                                                \
	X(Input)                                                                   \
	X(Texture)

#define DEFINE_ADD_COMPONENT_FUNCTION(name)                                    \
	void add_component(MemoryHeader *h, size_t entity_id, name component) {    \
		size_t i = h->p##name##s->count;                                       \
		h->p##name##s->entity_ids[i] = entity_id;                              \
		h->p##name##s->components[i] = component;                              \
		h->p##name##s->count++;                                                \
		h->world.component_masks[entity_id] |= name##Component;                \
	}

#define X(name) DEFINE_ADD_COMPONENT_FUNCTION(name)
SUBKEY_TYPES
#undef X