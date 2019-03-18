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


void showFbList() {
	struct fb *current_fb = *(struct fb **)get_memory_adr();
	while (current_fb != NULL) {
		printf("Block Libre de %ld à %ld suivant à: %ld\n",
			(void *)current_fb - get_memory_adr(), (void *)current_fb + (current_fb->size & SIZE_MASK) - get_memory_adr(), (void *)(current_fb->next)-get_memory_adr());
		if (current_fb->next <= current_fb) {
			printf("Boucle detectée ou fin atteinte\n");
			return;
		} else {
			current_fb = current_fb->next;
		}
	}
}

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
		print(ptr, *(size_t *)ptr & SIZE_MASK, *(size_t *)ptr & FREE_MASK);
		ptr = ptr + (*(size_t *)ptr & SIZE_MASK);
		//printf("ptr = %ld end = %ld\n", ptr - get_memory_adr(), ptr_end - get_memory_adr());
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
	struct fb *fb=mem_fit_fn(*(struct fb **)get_memory_adr(), taille); //On trouve le bloc libre adéquat

	if (fb == NULL) //si aucun bloc n'est libre on return NULL
		return NULL;

	//On met à jour la taille pour coller à l'alignement. NB: C'est le deuxième appel de cette fct en un allocation, on pourrait sans doute s'en passer
	taille += get_padding(taille, (void *)fb + (fb->size & SIZE_MASK)) + sizeof(size_t);
	fb ->size -= taille;
	if ((fb->size & SIZE_MASK) == 0) { //Le block n'est plus libre du tout (aligné sur 16 il restera tjrs l'entête, il faut s'en débarasser)
		printf("block rempli\n");
		struct fb **p_first_fb = (struct fb **)get_memory_adr();
		//Si le block libre est rempli, on met à jour la liste des block
		if (*p_first_fb == fb) {
			printf("premier block\n");
			*p_first_fb = fb->next;
		} else {
			printf("pas premier block\n");
			struct fb *current_fb = *p_first_fb;
			while (current_fb->next != fb)
				current_fb = current_fb->next;
			current_fb->next = fb->next;
		}
		showFbList();
		*(size_t *)((void *)fb + (fb->size & SIZE_MASK)) = taille;
		return (void *)fb + sizeof(size_t);
	}
	showFbList();
	*(size_t *)((void *)fb + (fb->size & SIZE_MASK)) = taille;
	return (void *)((void *)fb + (fb->size & SIZE_MASK) + sizeof(size_t));
}


void mem_free(void* mem) {
	struct fb *prev_fb = NULL;
	struct fb *first_fb = *(struct fb **)get_memory_adr();
	struct fb *newFb;
	void *block = (size_t *)(get_memory_adr() + sizeof(struct fb *)); //On récupère le premier block

	while (block + sizeof(size_t) < mem) {
		if ((*(size_t *)block) & FREE_MASK) {//On garde trace du dernier bloc libre parcouru
			prev_fb = (struct fb *)block;
			printf("block libre à %ld\n", block - get_memory_adr());
		}
		block += (*(size_t *)block) & SIZE_MASK;	//On continue jusqu'au bloc suivant
	}

	if (block + sizeof(size_t) != mem || (*(size_t *)block) & FREE_MASK)
		return ; //Si le bon block n'existe pas ou ce block est déjà libre

	if (prev_fb == NULL) { //block sera le premier block libre
		printf("Premier block libre\n");
		newFb = (struct fb *)block;
		if (first_fb != NULL && block + newFb->size == (void *)first_fb) {
			//le bloc est suivi du premier bloc libre
			printf("Suivi du premier block");
			newFb->size += first_fb->size;
			newFb->next = first_fb->next;
		} else {
			//Les bloc n'est pas suivi du premier bloc libre
			printf("non Suivi du premier block\n");
			newFb->size += 1; //On indique que le bloc est libre
			newFb->next = first_fb;
		}
		*(struct fb **)get_memory_adr() = block;
	} else { //block n'est pas le premier block libre
		printf("N'est pas premier block Libre\n");
		printf("Le précédent finit à: %ld \n", (void *)prev_fb + (prev_fb->size & SIZE_MASK) - get_memory_adr());
		if ((void *)prev_fb + (prev_fb->size & SIZE_MASK) == block) { //block est précédé d'un block libre
			prev_fb->size += *(size_t *)block;
			newFb = prev_fb;
		} else {	//Sinon on crée un nouveau block
			newFb = (struct fb *)block;
			newFb->size += 1;
			newFb->next = prev_fb->next;
			prev_fb->next = newFb;
		}
		printf("nouveau block de %ld à %ld\n", (void *)newFb - get_memory_adr(), (void *)newFb + (newFb->size & SIZE_MASK) - get_memory_adr());
		if ((void *)newFb + (newFb->size & SIZE_MASK) == (void *)newFb->next) { //Si nécessaire, fusion avec le block suivant
			newFb->size += newFb->next->size - 1;
			newFb->next = newFb->next->next;
		}
	}
	showFbList();
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
