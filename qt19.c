/* SO 37577522 */
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "stderr.h"
#include "emalloc.h"

static const double CTR_X =    0.0;
static const double CTR_Y =    0.0;
static const double WIDTH = +100.0;

typedef struct Particle Particle;
typedef struct Node Node;
typedef struct Area Area;
typedef struct ParticleArray ParticleArray;

struct Particle
{
    double mass;
    double x_pos;
    double y_pos;
    double x_vel;
    double y_vel;
};

struct Area
{
    double center_x;
    double center_y;
    double width;
};

struct ParticleArray
{
    size_t     max_idx;
    size_t     cur_idx;
    Particle **array;
};

struct Node
{
    Particle *p;
    Area      a;
    Node     *sw;
    Node     *nw;
    Node     *se;
    Node     *ne;
};

/* Debugging/diagnostic code */

static void print_area(const char *tag, const Area *a)
{
    printf("%s: C (%6.2f,%6.2f) W %6.2f", tag, a->center_x, a->center_y, a->width);
}

static void print_particle(int i, const Particle *p)
{
    printf("# Particle %d: (%6.2f,%6.2f) M %6.2f V (%6.2f,%6.2f)\n",
           i, p->x_pos, p->y_pos, p->mass, p->x_vel, p->y_vel);
}

static inline void print_quadtree_info(const char *tag, const Node *n)
{
    printf("%s: C (%6.2f,%6.2f) W %6.2f\n", tag, n->a.center_x, n->a.center_y, n->a.width);
    if (n->p)
        print_particle(-1, n->p);
}

static inline void print_quadtree_node(Node *n)
{
    print_quadtree_info("Box", n);
    if (n->p)
        print_particle(0, n->p);
    if (n->nw)
        print_quadtree_info(" NW", n->nw);
    if (n->sw)
        print_quadtree_info(" SW", n->sw);
    if (n->se)
        print_quadtree_info(" SE", n->se);
    if (n->ne)
        print_quadtree_info(" NE", n->ne);
}

static inline void print_quadtree_details(const char *tag, Node *n, int d)
{
    static const char indent[] =
        "                                                            ";
    assert(d < 15);
    const char *prefix = indent + sizeof(indent) - d * 3;
    printf("%s%s Centre (%6.2f,%6.2f) W %6.2f ",
           prefix, tag, n->a.center_x, n->a.center_y, n->a.width);
    if (n->p != 0)
        printf("Point (%6.2f,%6.2f)", n->p->x_pos, n->p->y_pos);
    putchar('\n');
    if (n->nw != 0)
        print_quadtree_details("NW:", n->nw, d+1);
    if (n->sw != 0)
        print_quadtree_details("SW:", n->sw, d+1);
    if (n->se != 0)
        print_quadtree_details("SE:", n->se, d+1);
    if (n->ne != 0)
        print_quadtree_details("NE:", n->ne, d+1);
}

static inline void print_quadtree(Node *n)
{
    assert(n != 0);
    printf("QT Node %p\n", (void *)n);
    print_quadtree_details("===", n, 0);
}
/* End Debugging/Diagnostic code */

static inline bool boxes_overlap(const Area *a1, const Area *a2)
{
    /* If boxes overlap, they overlap in x-dimension and y-dimension */
    /* x1.low <= x2.high && x1.high >= x2.low */
    return (a1->center_x - a1->width <= a2->center_x + a2->width &&
            a1->center_x + a1->width >= a2->center_x - a2->width &&
            a1->center_y - a1->width <= a2->center_y + a2->width &&
            a1->center_y + a1->width >= a2->center_y - a2->width);
}

static inline bool in_box(const Particle *p, const Area *box)
{
    return((p->x_pos >= box->center_x - box->width && p->x_pos < box->center_x + box->width) &&
           (p->y_pos >= box->center_y - box->width && p->y_pos < box->center_y + box->width));
}

static void check_in_box(const Particle *p, const Area *box)
{
    if (!in_box(p, box))
    {
        fprintf(stderr, "Point (%6.2f,%6.2f) is outside rectangle (%6.2f,%6.2f) W %6.2f\n",
                p->x_pos, p->y_pos, box->center_x, box->center_y, box->width);
        exit(EXIT_FAILURE);
    }
}

static Node *make_node(double center_x, double center_y, double width)
{
    Node *n = MALLOC(sizeof(*n));
    n->a.center_x = center_x;
    n->a.center_y = center_y;
    n->a.width = width;
    n->p = 0;
    n->sw = 0;
    n->se = 0;
    n->ne = 0;
    n->nw = 0;
    return n;
}

/* Node can be leaf node with no particle */
/* Node can be leaf node with a particle */
/* Node can be non-leaf node with no particle */
/* Leaf nodes have all sector pointers null */
/* Non-leaf nodes have all sector pointers not null */
static inline void check_node(Node *n)
{
    if (n != 0)
        assert((n->p == 0 && (n->nw != 0 && n->sw != 0 && n->se != 0 && n->ne != 0)) ||
               (n->p == 0 && (n->nw == 0 && n->sw == 0 && n->se == 0 && n->ne == 0)) ||
               (n->p != 0 && (n->nw == 0 && n->sw == 0 && n->se == 0 && n->ne == 0)));
}

static void split_node(Node *n)
{
    assert(n != 0);
    printf("= Split node: (%6.2f,%6.2f) W %6.2f\n", n->a.center_x, n->a.center_y, n->a.width);
    assert(n != 0 && n->sw == 0 && n->nw == 0 && n->ne == 0 && n->se == 0 && n->p != 0);
    double new_width = n->a.width / 2.0;
    n->sw = make_node(n->a.center_x - new_width, n->a.center_y - new_width, new_width);
    n->nw = make_node(n->a.center_x - new_width, n->a.center_y + new_width, new_width);
    n->se = make_node(n->a.center_x + new_width, n->a.center_y - new_width, new_width);
    n->ne = make_node(n->a.center_x + new_width, n->a.center_y + new_width, new_width);
    Particle *p = n->p;
    if (in_box(p, &n->sw->a))
        n->sw->p = p;
    else if (in_box(p, &n->nw->a))
        n->nw->p = p;
    else if (in_box(p, &n->se->a))
        n->se->p = p;
    else if (in_box(p, &n->ne->a))
        n->ne->p = p;
    else
    {
        printf("*** Assert firing!\n");
        print_particle(-2, p);
        print_quadtree_node(n);
        assert(0);  /* Can't happen! */
        abort();
    }
    n->p = 0;
    check_node(n);
}

static Node *quadtree_insert(Node *n, struct Particle *p, const Area *box)
{
    printf("Point (%6.2f,%6.2f), Centre (%6.2f,%6.2f), W = %6.2f\n",
            p->x_pos, p->y_pos, box->center_x, box->center_y, box->width);
    check_node(n);
    /* Check that point falls in bounding box */
    check_in_box(p, box);
    if (n == NULL)
    {
        n = make_node(box->center_x, box->center_y, box->width);
        n->p = p;
    }
    else if (n->nw == 0 && n->p == 0)
    {
        n->p = p;
    }
    else
    {
        if (n->nw == 0)  // Could check any segment for null-ness
        {
            /* Need to split node into four and insert particle into correct node */
            assert(n->p != 0);
            split_node(n);
            //print_quadtree_node(n);
        }

        double new_width = box->width * 0.5;
        if (p->x_pos < box->center_x && p->y_pos < box->center_y)
        {
            printf("Recurse SW 1: ");
            Area new_box = { box->center_x - new_width, box->center_y - new_width, new_width };
            Node *sw = quadtree_insert(n->sw, p, &new_box);
            assert(sw == n->sw);
            check_node(sw);
        }
        else if (p->x_pos < box->center_x && p->y_pos >= box->center_y)
        {
            printf("Recurse NW 1: ");
            Area new_box = { box->center_x - new_width, box->center_y + new_width, new_width };
            Node *nw = quadtree_insert(n->nw, p, &new_box);
            assert(nw == n->nw);
            check_node(nw);
        }
        else if (p->x_pos >= box->center_x && p->y_pos < box->center_y)
        {
            printf("Recurse SE 1: ");
            Area new_box = { box->center_x + new_width, box->center_y - new_width, new_width };
            Node *se = quadtree_insert(n->se, p, &new_box);
            assert(se == n->se);
            check_node(se);
        }
        else
        {
            printf("Recurse NE 1: ");
            Area new_box = { box->center_x + new_width, box->center_y + new_width, new_width };
            Node *ne = quadtree_insert(n->ne, p, &new_box);
            assert(ne == n->ne);
            check_node(ne);
        }
    }
    check_node(n);
    return n;
}

static void free_quadtree(Node *n, bool free_particles)
{
    if (n != 0)
    {
        if (free_particles)
            free(n->p);
        free_quadtree(n->nw, free_particles);
        free_quadtree(n->sw, free_particles);
        free_quadtree(n->se, free_particles);
        free_quadtree(n->ne, free_particles);
        free(n);
    }
}

static void add_particle_to_array(ParticleArray *r, Particle *p)
{
    //printf("%s", __func__);
    //print_particle(0, p);
    if (r->cur_idx >= r->max_idx)
    {
        size_t new_max = r->max_idx * 2 + 2;
        Particle **new_arr = REALLOC(r->array, new_max * sizeof(*new_arr));
        /* REALLOC never returns a null pointer */
        r->array = new_arr;
        r->max_idx = new_max;
    }
    r->array[r->cur_idx++] = p;
}

static ParticleArray *make_particle_array(void)
{
    ParticleArray *r = MALLOC(sizeof(*r));
    r->max_idx = 0;
    r->cur_idx = 0;
    r->array = 0;
    return r;
}

static void reset_particle_array(ParticleArray *r)
{
    r->cur_idx = 0;
}

static void free_particle_array(ParticleArray *r)
{
    if (r != 0)
    {
        free(r->array);
        free(r);
    }
}

static size_t quadtree_search(const Node *n, const Area *a, ParticleArray *r)
{
    //printf("-->> %s", __func__);
    //print_area(" A", a);
    //print_area(" N", &n->a);
    //printf(" %s\n", boxes_overlap(&n->a, a) ? "overlap" : "disjoint");
    size_t count = 0;
    if (boxes_overlap(&n->a, a))
    {
        if (n->p != 0)
        {
            //print_area("A", a);
            //print_particle(0, n->p);
            if (in_box(n->p, a))
            {
                add_particle_to_array(r, n->p);
                count++;
            }
        }
        else if (n->nw != 0)
        {
            count += quadtree_search(n->nw, a, r);
            count += quadtree_search(n->sw, a, r);
            count += quadtree_search(n->se, a, r);
            count += quadtree_search(n->ne, a, r);
        }
    }
    //printf("<<-- %s (%zu)\n", __func__, count);
    return count;
}

static void print_particle_array(const char *tag, const ParticleArray *r)
{
    if (r->cur_idx > 0)
    {
        printf("%s: (%zu)\n", tag, r->cur_idx);
        for (size_t i = 0; i < r->cur_idx; i++)
            print_particle(i, r->array[i]);
    }
}

/* Test code */

static void built_in(void)
{
    /* random -n 10 -T '    { %6:2[1:20]f, %6:2[-100:100]f, %6:2[-100:100]f, %6:2[-100:100]f, %6:2[-100:100]f },' */
    static struct Particle particles[] =
    {
        {  19.99,  96.07,  62.79, -99.46,  19.70 },
        {  12.94,   1.43, -33.45,  31.80, -66.08 },
        {   6.49,  16.99, -20.83,  92.51,  35.98 },
        {  17.01, -28.85, -94.10,  42.82,  -1.30 },
        {  14.27,  85.07,  88.21,  11.22,  16.85 },
        {  15.73, -56.37,  46.85,  27.40, -15.15 },
        {   1.53, -49.44, -64.27, -29.45, -38.25 },
        {   8.03,  92.11, -47.50,  63.77, -29.99 },
        {   8.67, -99.81,  73.19,  18.75,  88.66 },
        {  16.36,  66.33,  14.23,  87.65,  40.01 },
    };
    enum { nParticles = sizeof(particles) / sizeof(particles[0]) };
    Node *root = NULL;
    Area  a = { CTR_X, CTR_Y, WIDTH };

    printf("# Particle 0: ");
    root = quadtree_insert(root, &particles[0], &a);
    print_quadtree(root);

    for (int i = 1; i < nParticles; i++)
    {
        printf("# Particle %d: ", i);
        Node *tree = quadtree_insert(root, &particles[i], &a);
        assert(tree == root);
        print_quadtree(root);
    }
    free_quadtree(root, false); // Do not free particles
}

static void test_search(const Node *root, int num)
{
    assert(num > 0 && num <= 100);
    printf("Search in %dx%d sections\n", num, num);
    ParticleArray *r = make_particle_array();
    for (int i = 0; i < num; i++)
    {
        double x = CTR_X - WIDTH + (WIDTH / num) + i * (2.0 * WIDTH / num);
        for (int j = 0; j < num; j++)
        {
            double y = CTR_Y - WIDTH + (WIDTH / num) + j * (2.0 * WIDTH / num);
            reset_particle_array(r);
            Area box = { x, y, WIDTH / num };
            size_t n = quadtree_search(root, &box, r);
            printf("Found %zu points in ", n);
            print_area("search area", &box);
            putchar('\n');
            print_particle_array("Points in box", r);
        }
    }
    free_particle_array(r);
}

static int read_particle(FILE *fp, Particle *p)
{
    char  *buffer = 0;
    size_t buflen = 0;
    while (getline(&buffer, &buflen, fp) != -1)
    {
        /* Skip empty lines (newline only) and comment lines (# in column 1) */
        if (buffer[0] == '#' || buffer[0] == '\n')
            continue;
        if (sscanf(buffer, "%lf %lf %lf %lf %lf",
                   &p->mass, &p->x_pos, &p->y_pos, &p->x_vel, &p->y_vel) == 5)
        {
            free(buffer);
            return 0;
        }
        err_error("Failed to extract 5 numbers from line:\n%s", buffer);
    }
    free(buffer);
    return EOF;
}

static void read_from_file(const char *file)
{
    printf("Data from: %s\n", file);
    FILE *fp = fopen(file, "r");
    if (fp == 0)
        err_syserr("Failed to open file %s: ", file);
    Node *root = NULL;
    Particle particle;
    Area box = { CTR_X, CTR_Y, WIDTH };

    for (int i = 0; read_particle(fp, &particle) != EOF; i++)
    {
        Particle *p = MALLOC(sizeof(*p));
        *p = particle;
        //print_particle(i, p);
        Node *tree = quadtree_insert(root, p, &box);
        if (root == NULL)
            root = tree;
        assert(tree == root);
        //print_quadtree(root);
    }

    print_quadtree(root);

    test_search(root, 2);
    test_search(root, 3);
    test_search(root, 4);

    free_quadtree(root, true);  // Free particles too

    fclose(fp);
}

static void test_overlap(void)
{
    Area boxes[] =
    {
        {  0.0,  0.0, 10.0 },
        { 80.0, 80.0, 10.0 },
        { 70.0, 70.0, 10.0 },
        { 75.0, 65.0, 10.0 },
        { 45.0, 95.0,  5.0 },
        { 56.0, 95.0,  5.0 },
        { 55.0, 95.0,  5.0 },
    };
    enum { NUM_BOXES = sizeof(boxes) / sizeof(boxes[0]) };

    for (int i = 0; i < NUM_BOXES; i++)
    {
        char buffer1[32];
        snprintf(buffer1, sizeof(buffer1), "Area %d", i);
        print_area(buffer1, &boxes[i]);
        putchar('\n');
        for (int j = 0; j < NUM_BOXES; j++)
        {
            char buffer2[32];
            snprintf(buffer2, sizeof(buffer2), "    Area %d", j);
            print_area(buffer2, &boxes[j]);
            printf((boxes_overlap(&boxes[i], &boxes[j])) ? " overlap" : " disjoint");
            putchar('\n');
        }
    }
}

int main(int argc, char **argv)
{
    err_setarg0(argv[0]);

    if (argc == 0)
        test_overlap();

    if (argc == 1)
    {
        printf("Built-in Data:\n");
        built_in();
    }
    else
    {
        for (int i = 1; i < argc; i++)
            read_from_file(argv[i]);
    }
    return 0;
}

/*
Notes towards generalization:
 X. Add structure to describe squares. -- Done!
 2. Allow command line scaling of region.
 3. Consider whether auto-allocation of all 4 nodes on split is OK.
 X. Add a search function as in example code at Wikipedia
    (https://en.wikipedia.org/wiki/Quadtree). -- Done!
 5. Consider whether to allow printing of quadtree after every insert,
    and other verbosity command line options).
 6. Consider moving to vignettes.
 7. Consider whether mass and velocity is still relevant (not really).
 8. Improved identification/tagging for print_particle() -- plain
    print_particle() to take a general string tag, and
    print_particle_num() to print a number.
 9. Consider allowing more than one point per quadtree node (like the
    code at Wikipedia does).
10. Support rectangular shapes.
11. Specify searchable ranges from command line.
12. Generalize division of ranges.
13. Use ffilter() to handle reading from files or standard input, with
    option to run built-in test (and another to run the overlap test,
    and another to run a contains (in_box()) test).
14. Consider how to measure performance of quadtree search against
    linear search.
15. Determine when quadtree search outperforms linear search.
*/
