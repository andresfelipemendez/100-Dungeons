struct Position {
	float x;
};
if(mask & PositionComponent) {
	Position position;
	if (get_component(m, entity_id, &position)) {
		fprintf(fp,"position = { x = %.2f }", position.x);
	}
}