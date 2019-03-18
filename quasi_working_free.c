void mem_free(void* mem) {
	struct fb *last_fb = NULL;
	void *block = (void *)(get_memory_adr() + sizeof(struct fb *));
	void *ptr_end = get_memory_adr() + get_memory_size();

	//On parcours tout les blocs en mémoire jusqu'a trouver le bon
	//On garde en mémoire le dernier bloc libre (utile par la suite)
	while (block + sizeof(size_t) < mem && block + sizeof(size_t) < ptr_end) {
		if (*(size_t *)block & FREE_MASK)
			last_fb = (struct fb *)block;
		block = block + (*(size_t *)block & SIZE_MASK);
	}

	if (block + sizeof(size_t) != mem || *(size_t *)block & FREE_MASK)
		return ;	//Si le bloc est introuvable on quite la fct

	if (last_fb != NULL) { //Si un block libre existe avant l bloc à libérer
		if ((void *)last_fb + last_fb ->size == block) { //Si celui ci est placé juste avant le block à libérer
			last_fb ->size += *(size_t *)block;
			//TODO: Fusionner avec le block suivant si celui ci est accolé
			return ;
		}

		if (block + *(size_t *)block == (void *)(last_fb ->next)) { //Si il y a un block juste après est juste après
			//On crée un nouveau block libre à la place du block à libérer et on le fusion avec celui qui suit
			((struct fb *)block) ->next = last_fb ->next ->next;
			((struct fb *)block) ->size = *(size_t *)block+ last_fb ->next ->size;
			last_fb ->next = (struct fb *)block;
			return;
		}

		//Le bloc est seul et abandonné de tous au milieu d'autres blocs occupés
		((struct fb *)block) ->next = last_fb ->next ->next;
		((struct fb *)block) ->size = *(size_t *)block + 1;
		last_fb ->next = (struct fb *)block;
		return;
	} else {//On apas trouvé de bloc libre avant, le bloc à libérer deviendra le nouveau bloc libre
		//Le premier bloc est situé juste après le bloc à libérer
		struct fb *first_fb = *(struct fb **)get_memory_adr();
		if (block + *(size_t *)block == (void *)first_fb) {
			((struct fb *)block) ->next = first_fb;
			((struct fb *)block) ->size = *(size_t *)block + first_fb->size;
			*(void **)get_memory_adr() = block;
			return;
		}

		//Le bloc est suivi d'un bloc occupé
		((struct fb *)block) ->next = first_fb;
		((struct fb *)block) ->size = *(size_t *)block + 1;
		*(void **)get_memory_adr() = block;
		//TODO Il peut ne plus y a voir de block libres du tout
		return;

	}

}
