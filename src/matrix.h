#ifndef MATRIX_H
#define MATRIX_H

#include <stdio.h>
#include <stdlib.h>

// sparse matrix: orthogonal doubly-linked lists.
// Each entry (i,j) is either 0 or 1.
// Rows and columns are doubly-linked lists of elements.
// Arrays rows[] and cols[] map integer indices to headers.

/* a single 1-entry in the matrix */
typedef struct sm_element
{
    int row_num, col_num;
    struct sm_element *next_row, *prev_row;
    struct sm_element *next_col, *prev_col;
} sm_element;

/* row header – owns a linked list of elements in this row */
typedef struct sm_row
{
    int row_num;
    int length; /* how many elements in this row */
    sm_element *first_col, *last_col;
    struct sm_row *next_row, *prev_row;
} sm_row;

/* column header – owns a linked list of elements in this column */
typedef struct sm_col
{
    int col_num;
    int length; /* how many elements in this column */
    sm_element *first_row, *last_row;
    struct sm_col *next_col, *prev_col;
} sm_col;

/* the sparse matrix itself */
typedef struct sm_matrix
{
    sm_row **rows; /* rows[i]  → row header (or NULL) */
    int rows_size; /* allocated size of rows[]        */
    sm_col **cols; /* cols[j] → column header (or NULL) */
    int cols_size;
    sm_row *first_row, *last_row;
    sm_col *first_col, *last_col;
    int nrows, ncols; /* logical row / column count      */
} sm_matrix;

// core API

sm_matrix *matrix_alloc(void);
void matrix_free(sm_matrix *A);

/* insert a 1 at (row,col); returns pointer to the element (existing
 * or newly created).  Automatically resizes the index arrays. */
sm_element *matrix_insert(sm_matrix *A, int row, int col);

/* find the element at (row,col), or NULL */
sm_element *matrix_find(sm_matrix *A, int row, int col);

/* remove the 1 at (row,col) if it exists */
void matrix_remove(sm_matrix *A, int row, int col);

/* ensure the row/col index arrays are large enough */
void matrix_resize(sm_matrix *A, int row, int col);

// accessors (all O(1))

static inline sm_row *matrix_get_row(sm_matrix *A, int r)
{
    return (r >= 0 && r < A->rows_size) ? A->rows[r] : NULL;
}
static inline sm_col *matrix_get_col(sm_matrix *A, int c)
{
    return (c >= 0 && c < A->cols_size) ? A->cols[c] : NULL;
}
static inline int matrix_row_count(sm_matrix *A, int r)
{
    sm_row *pr = matrix_get_row(A, r);
    return pr ? pr->length : 0;
}
static inline int matrix_col_count(sm_matrix *A, int c)
{
    sm_col *pc = matrix_get_col(A, c);
    return pc ? pc->length : 0;
}

// iteration macros

// iterate all rows
#define sm_foreach_row(A, prow) \
    for ((prow) = (A)->first_row; (prow) != NULL; (prow) = (prow)->next_row)

// iterate all columns
#define sm_foreach_col(A, pcol) \
    for ((pcol) = (A)->first_col; (pcol) != NULL; (pcol) = (pcol)->next_col)

// iterate elements in a row
#define sm_foreach_row_element(prow, p) \
    for ((p) = (prow)->first_col; (p) != NULL; (p) = (p)->next_col)

// iterate elements in a column
#define sm_foreach_col_element(pcol, p) \
    for ((p) = (pcol)->first_row; (p) != NULL; (p) = (p)->next_row)

#endif /* MATRIX_H */
