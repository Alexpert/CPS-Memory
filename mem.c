#include "mem.h"
#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

// constante définie dans gcc seulement
#ifdef __BIGGEST_ALIGNMENT__
#define ALIGNMENT __BIGGEST_ALIGNMENT__
#else
#define ALIGNMENT 16
#endif

#define SIZE_MASK 0xFFFFFFFFFFFFFFFE
#define FREE_MASK 0x0000000000000001


struct fb {
	/*
	Le bit de poids faible de size n'est jamais utilisé. il sera donc initilaisé à 1 pour un bloc libre et à 0 pour un block occupé
	size comprends sizeof(struct fb) + l'espace libre qui le suit
	*/
	size_t size;
	struct fb* next;
};


void mem_init(void* mem, size_t taille)
{
	assert(mem == get_memory_adr());
	assert(taille == get_memory_size());

	printf("mem at: %ld\n", get_memory_adr() - get_memory_adr());
	printf("size  : %ld\n", get_memory_size());

	struct fb first_fb = {get_memory_size() - sizeof(struct fb *) + 1, NULL}; //Premier bloc libre
	*(struct fb **)mem = (struct fb *)(mem + sizeof(struct fb *));						//On stock le pointeur du prmier fb à l'adresse mem
	**(struct fb **)mem = first_fb;																						//ON met le premier bloc à son emplacement en mémoire

	/*
		[first_fb * | fb -> size | fb ->next| XXXXXXXXXXX ]
		|<- mem     |<- fb									|<- libre
	*/

	mem_fit(&mem_fit_first);
}

void mem_show(void (*print)(void *, size_t, int)) {
	void *ptr = (void *)(get_memory_adr() + sizeof(struct fb *)); //on trouve le pointeur du premier bloc de la mémoire
	void *ptr_end = get_memory_adr() + get_memory_size();					//on trouve le pointeur de fin de la mémoire (celui après le dernier)

	//précision sur la boucle qui suit: les blocks libre et occupés, désignés par
	//des (size_t *) se comporte implicitement comme une liste chainée car
	//pour un block p donnée, p + la taille de son contenu donne le pointeur
	//du block suivant
	while (ptr < ptr_end) {
		// print(ptr, *(size_t *)ptr & SIZE_MASK, *(size_t *)ptr & FREE_MASK);
		ptr = ptr + (*(size_t *)ptr & SIZE_MASK);
		// printf("ptr = %ld end = %ld\n", ptr - get_memory_adr(), ptr_end - get_memory_adr());
	}
}

static mem_fit_function_t *mem_fit_fn;
void mem_fit(mem_fit_function_t *f) {
	mem_fit_fn=f;
}

size_t get_padding(size_t size, void *ptr_end) {
	// Concernant le masque:
	// Soit ALIGN = 16; 16 = 0x0010
	// ALIGN-1 =  16-1 = 15; 15 = 0x000F
	// ~(ALIGN - 1) = ~0x000F = 0xFFF0
	unsigned long int mask = ~(ALIGNMENT - 1);
	printf("mask %lX\n", mask);
	void *ptr_start = (void *)((unsigned long int)(ptr_end - size) & mask);	//Alignement sur ALIGN du pointeur
	size_t padd =  ptr_end - ptr_start - size;
	printf("padding: %ld\n adress: %ld to %ld\n", padd, ptr_start-get_memory_adr(), ptr_end-get_memory_adr());
	return padd;
}

void *mem_alloc(size_t taille) {
	/* ... */
	__attribute__((unused)) /* juste pour que gcc compile ce squelette avec -Werror */
	struct fb *fb=mem_fit_fn(*(struct fb **)get_memory_adr(), taille); //On trouve le bloc libre adéquat

	if (fb == NULL) //si aucun bloc n'est libre on return NULL
		return NULL;

	//On met à jour la taille pour coller à l'alignement. NB: C'est le deuxième appel de cette fct en un allocation, on pourrait sans doute s'en passer
	taille += get_padding(taille, (void *)fb + (fb->size & SIZE_MASK)) + sizeof(size_t);
	fb ->size -= taille;
	*(size_t *)((void *)fb + (fb->size & SIZE_MASK)) = taille;

	return (void *)((void *)fb + (fb->size & SIZE_MASK) + sizeof(size_t));
}


void mem_free(void* mem) {
	struct fb *last_fb = NULL;
	void *block = (void *)(get_memory_adr() + sizeof(struct fb *));
	void *ptr_end = get_memory_adr() + get_memory_size();

	while (block + sizeof(size_t) < mem && block + sizeof(size_t) < ptr_end) {
		if (*(size_t *)block & FREE_MASK)
			last_fb = (struct fb *)block;
		block = block + (*(size_t *)block & SIZE_MASK);
	}

	if (block + sizeof(size_t) != mem || *(size_t *)block & FREE_MASK)
		return ;

	if (last_fb != NULL) {
		if ((void *)last_fb + last_fb ->size == block) {
			last_fb ->size += *block;
			return ;
		}

		if (block + *block == (void *)(last_fb ->next)) {
			((struct fb *)block) ->next = last_fb ->next ->next;
			((struct fb *)block) ->size = *block + last_fb ->next ->size;
			last_fb ->next = (struct fb *)block;
		}

	} else {

	}

}


struct fb* mem_fit_first(struct fb *list, size_t size) {
	int valide = 0, effective_size = 0;
	struct fb *fb = list;
	while (fb != NULL && !valide) {
		effective_size = size + get_padding(size, (void *)((void *)fb + (fb ->size & SIZE_MASK)));
		if (fb ->size > effective_size + sizeof(size_t))
			valide = 1;
		else
			fb = fb ->next;
	}
	return fb;
}

/* Fonction à faire dans un second temps
 * - utilisée par realloc() dans malloc_stub.c
 * - nécessaire pour remplacer l'allocateur de la libc
 * - donc nécessaire pour 'make test_ls'
 * Lire malloc_stub.c pour comprendre son utilisation
 * (ou en discuter avec l'enseignant)
 */
size_t mem_get_size(void *zone) {
	/* zone est une adresse qui a été retournée par mem_alloc() */

	/* la valeur retournée doit être la taille maximale que
	 * l'utilisateur peut utiliser dans cette zone */
	zone -= sizeof(size_t);
	return *(size_t *)zone;
}

/* Fonctions facultatives
 * autres stratégies d'allocation
 */
struct fb* mem_fit_best(struct fb *list, size_t size) {
	return NULL;
}

struct fb* mem_fit_worst(struct fb *list, size_t size) {
	return NULL;
}
