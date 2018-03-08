#include "dgtrealmp_private.h"

LTFAT_REAL
LTFAT_NAME(pedantic_callback)(void* userdata,
                              LTFAT_COMPLEX cval, ltfat_int pos)
{
    LTFAT_NAME(dgtrealmp_state_closure)* p =
        (LTFAT_NAME(dgtrealmp_state_closure)*) userdata;
    LTFAT_COMPLEX cvaldual;
    LTFAT_REAL projenergy;
    LTFAT_NAME(dgtrealmp_execute_dualprodandprojenergy)(
        p->state, kpoint_init(pos,p->n,p->w), cval, &cvaldual, &projenergy);

    return projenergy;
}


LTFAT_API int
LTFAT_NAME(dgtrealmp_init)(
    LTFAT_NAME(dgtrealmp_parbuf)* pb, ltfat_int L,
    LTFAT_NAME(dgtrealmp_state)** pout)
{
    int status = LTFATERR_FAILED;

    CHECKNULL(pb);
    CHECK(LTFATERR_BADARG,
          L > 0 , "Signal length L must be positive (passed %td)", L);
    CHECK(LTFATERR_BADARG, pb->P > 0 , "No Gabor system set in the plan");

    CHECKSTATUS( LTFAT_NAME(dgtrealmp_init_gen)(
               (const LTFAT_REAL**)pb->g, pb->gl, L, pb->P, pb->a, pb->M,
               pb->params, pout));

    memcpy((*pout)->chanmask, pb->chanmask, pb->P*sizeof*pb->chanmask);
    return LTFATERR_SUCCESS;
error:
    return status;
}

LTFAT_API int
LTFAT_NAME(dgtrealmp_init_gen)(
    const LTFAT_REAL* g[], ltfat_int gl[], ltfat_int L, ltfat_int P, ltfat_int a[],
    ltfat_int M[], ltfat_dgtmp_params* params, LTFAT_NAME(dgtrealmp_state)** pout)
{
    int status = LTFATERR_FAILED;
    const LTFAT_REAL* gtmp[2]; ltfat_int gltmp[2]; ltfat_int atmp[2];
    ltfat_int Mtmp[2];
    ltfat_int nextL;
    ltfat_int amin;
    LTFAT_NAME(dgtrealmp_state)* p = NULL;
    ltfat_dgt_params* dgtparams = NULL;

    CHECK(LTFATERR_NOTPOSARG, P > 0, "P must be positive (passed %td)", P);
    CHECK(LTFATERR_NOTPOSARG, L > 0, "L must be positive (passed %td)", L);
    CHECKNULL(gl); CHECKNULL(g); CHECKNULL(a); CHECKNULL(M); CHECKNULL(pout);


    for (ltfat_int p = 0; p < P; p++)
    {
        CHECKNULL(g[p]);
        CHECK(LTFATERR_NOTPOSARG, gl[p] > 0,
              "gl[%td] must be positive (passed %td)", p, gl[p]);
        CHECK(LTFATERR_NOTPOSARG, a[p] > 0,
              "a[%td] must be positive (passed %td)", p, a[p]);
        CHECK(LTFATERR_NOTPOSARG, M[p] > 0,
              "M[%td] must be positive (passed %td)", p, M[p]);
        CHECK(LTFATERR_NOTAFRAME, M[p] >= a[p],
              "M[%td]>=a[%td] failed passed (%td,%td)", p, p,  M[p], a[p]);

        CHECK(LTFATERR_BADARG, gl[p] <= L,
              "gl[%td]<=L failed. Window is too long. passed (%td, %td)",
              p, gl[p], L);

        LTFAT_REAL gnorm; LTFAT_NAME(norm)(g[p], gl[p], LTFAT_NORM_ENERGY, &gnorm);
        CHECK(LTFATERR_BADARG, ltfat_abs(gnorm - 1.0) < 1e-4,
              "Window g[%td] is not normalized. The norm is %.3f.", p, gnorm);
    }

    amin = a[0];
    for (ltfat_int p = 1; p < P; p++)
        if (a[p] < amin )
            amin = a[p];

    for (ltfat_int p = 0; p < P; p++)
    {
        CHECK( LTFATERR_BADARG, a[p] % amin == 0,
               "a[%td] not divisible by amin %td (passed %td)", p, amin, a[p]);
        CHECK( LTFATERR_BADARG, M[p] % amin == 0,
               "M[%td] not divisible by amin %td (passed %td)", p, amin, M[p]);
    }

    nextL = ltfat_dgtlengthmulti(L, P, a, M);

    CHECK(LTFATERR_BADTRALEN, L == nextL,
          "Next compatible transform length is %d (passed %d).", nextL, L);

    CHECKMEM( p = LTFAT_NEW( LTFAT_NAME(dgtrealmp_state)) );
    CHECKMEM( p->params = ltfat_dgtmp_params_allocdef() );

    if (params)
        memcpy( p->params, params, sizeof * p->params);

    if ( p->params->maxatoms == 0 )
        p->params->maxatoms =  (size_t) ( 0.1 * L);

    if ( p->params->maxit == 0 )
        p->params->maxit = 2 * p->params->maxatoms;

#ifdef NOBLASLAPACK
    CHECK( LTFATERR_NOBLASLAPACK,
           p->params->alg != ltfat_dgtmp_alg_LocOMP,
           "LocOMP requires LAPACK, but libltfat was compiled without it.");
#endif

    p->params->initwasrun = 1;

    CHECKMEM( p->dgtplans  = LTFAT_NEWARRAY( LTFAT_NAME(dgtreal_plan)*, P) );
    CHECKMEM( p->gramkerns = LTFAT_NEWARRAY( LTFAT_NAME(kerns)*, P * P) );
    CHECKMEM( p->a  = LTFAT_NEWARRAY( ltfat_int, P));
    CHECKMEM( p->M  = LTFAT_NEWARRAY( ltfat_int, P));
    CHECKMEM( p->M2 = LTFAT_NEWARRAY( ltfat_int, P));
    CHECKMEM( p->N  = LTFAT_NEWARRAY( ltfat_int, P));
    CHECKMEM( p->chanmask  = LTFAT_NEWARRAY( int, P));
    CHECKMEM( p->couttmp = LTFAT_NEWARRAY( LTFAT_COMPLEX*, P));

    for (ltfat_int k = 0; k < P; k++)
    {
        p->chanmask[k] = 1;
        p->a[k] = a[k]; p->M[k] = M[k];
        p->M2[k] = M[k] / 2 + 1; p->N[k] = L / a[k];
    }

    p->P = P; p->L = L;

    CHECKMEM( dgtparams = ltfat_dgt_params_allocdef());
    ltfat_dgt_setpar_phaseconv(dgtparams, p->params->ptype);
    ltfat_dgt_setpar_synoverwrites(dgtparams, 0);

    for (ltfat_int k = 0; k < P; k++)
    {
        CHECKSTATUS(
            LTFAT_NAME(dgtreal_init_gen)(g[k], gl[k], g[k], gl[k], L, 1, a[k], M[k],
                                         NULL, NULL, dgtparams, &p->dgtplans[k]));

    }
    ltfat_dgt_params_free(dgtparams); dgtparams = NULL;

    for (ltfat_int k1 = 0; k1 < P; k1++)
    {
        for (ltfat_int k2 = 0; k2 < P; k2++)
        {
            gtmp[0] = g[k1]; gtmp[1] = g[k2];
            atmp[0] = p->a[k1]; atmp[1] = p->a[k2];
            Mtmp[0] = p->M[k1]; Mtmp[1] = p->M[k2];
            gltmp[0] = gl[k1]; gltmp[1] = gl[k2];

            CHECKSTATUS( LTFAT_NAME(dgtrealmp_kernel_init)( gtmp, gltmp,
                         atmp, Mtmp, L, p->params->kernrelthr,
                         p->params->ptype,
                         &p->gramkerns[k1 + k2 * P]));
        }

        /* for(ltfat_int k2 = 0; k2 < k1; k2++) */
        /* { */
        /*     LTFAT_NAME(dgtrealmp_kernel_cloneconj)( */
        /*             p->gramkerns[k2 + k1 * P], */
        /*             &p->gramkerns[k1 + k2 * P]); */
        /* } */
    }

#ifndef NDEBUG
    /* for(ltfat_int kNo=0;kNo<P;kNo++) */
    /* { */
    /*     LTFAT_NAME(kerns)* kk = p->gramkerns[kNo]; */
    /*     printf("h=%td, w=%td \n", kk->size.height, kk->size.width); */
    /*     for (ltfat_int n = 0; n < kk->size.width; n++ ) */
    /*     { */
    /*         printf("s=%td,e=%td \n", kk->range[n].start, kk->range[n].end); */
    /*     } */
    /*  */
    /*     for (ltfat_int m = 0; m < kk->size.height; m++ ) */
    /*     { */
    /*         for (ltfat_int n = 0; n < kk->size.width; n++ ) */
    /*         { */
    /*             printf("r=% 5.3e,i=% 5.3e ", ltfat_real(kk->kval[n * kk->size.height + m]), */
    /*                    ltfat_imag(kk->kval[n * kk->size.height + m])); */
    /*         } */
    /*         printf("\n"); */
    /*     } */
    /* } */
#endif

    CHECKSTATUS( LTFAT_NAME(dgtrealmpiter_init)(a, M, P, L, &p->iterstate));

    if (p->params->alg == ltfat_dgtmp_alg_LocOMP)
    {
        ltfat_int kernSizeAccum = 0;
        for (ltfat_int k = 0; k < P; k++)
        {
            LTFAT_NAME(kerns)* ke = p->gramkerns[k];
            kernSizeAccum +=
                ltfat_idivceil( ke->size.width, ke->astep) *
                ltfat_idivceil(ke->size.height, ke->Mstep);
        }
        kernSizeAccum *= 2;

        CHECKMEM( p->iterstate->gramBuf =
                      LTFAT_NAME_COMPLEX(calloc)( kernSizeAccum * kernSizeAccum));
        CHECKMEM( p->iterstate->cvalBuf =
                      LTFAT_NAME_COMPLEX(calloc)( kernSizeAccum));
        CHECKMEM( p->iterstate->cvalinvBuf =
                      LTFAT_NAME_COMPLEX(calloc)( kernSizeAccum));
        CHECKMEM( p->iterstate->cvalBufPos =
                      LTFAT_NEWARRAY( kpoint, kernSizeAccum ));

        CHECKSTATUS(
            LTFAT_NAME_COMPLEX(hermsystemsolver_init)(
                kernSizeAccum, &p->iterstate->hplan));
    }

    if (p->params->alg == ltfat_dgtmp_alg_LocCyclicMP)
    {
        p->iterstate->pBufNo = p->params->maxatoms;
        p->iterstate->pBufNo = 0;
        CHECKMEM( p->iterstate->pBuf =
                      LTFAT_NEWARRAY( kpoint, p->params->maxatoms) );
    }

    if (p->params->ptype == LTFAT_FREQINV)
    {
        CHECKMEM(p->iterstate->cvalModBuf = LTFAT_NEWARRAY(LTFAT_COMPLEX*, P * P));
        for (ltfat_int k1 = 0; k1 < P; k1++)
        {
            for (ltfat_int k2 = 0; k2 < P; k2++)
            {
                LTFAT_NAME(kerns)* currkern = p->gramkerns[k1 + k2 * P];
                ltfat_int h2 = ltfat_idivceil( currkern->size.height , currkern->Mstep);
                CHECKMEM(p->iterstate->cvalModBuf[k1 + k2 * P] =
                             LTFAT_NAME_COMPLEX(malloc)( h2));
            }
        }
    }


    if(p->params->do_pedantic)
    {
        CHECKMEM(
            p->closures =
            LTFAT_NEWARRAY( LTFAT_NAME( dgtrealmp_state_closure)*, p->P));

        for (ltfat_int k = 0; k < p->P; k++)
        {
            CHECKMEM(
                p->closures[k] =
                LTFAT_NEWARRAY( LTFAT_NAME(dgtrealmp_state_closure), p->N[k]));

            for (ltfat_int n = 0; n < p->N[k]; n++ )
            {
                p->closures[k][n].n = n;
                p->closures[k][n].w = k;
                p->closures[k][n].state = p;

                CHECKSTATUS(
                LTFAT_NAME(maxtree_setcallback)(
                        p->iterstate->fmaxtree[k][n],
                        LTFAT_NAME(pedantic_callback),
                        &p->closures[k][n]));
            }
        }
    }

    *pout = p;
    return LTFATERR_SUCCESS;
error:
    if (p) LTFAT_NAME(dgtrealmp_done)(&p);
    if (dgtparams) ltfat_dgt_params_free(dgtparams);
    *pout = NULL;
    return status;
}


LTFAT_API int
LTFAT_NAME(dgtrealmp_reset)(LTFAT_NAME(dgtrealmp_state)* p, const LTFAT_REAL* f)
{
    int status = LTFATERR_SUCCESS;

    LTFAT_NAME(dgtrealmpiter_state)* istate = NULL;

    CHECKNULL(p); CHECKNULL(f);
    istate = p->iterstate;

    istate->currit = 0;
    istate->curratoms = 0;
    istate->err = 0.0;

    for (ltfat_int l = 0; l < p->L; l++)
        istate->err += f[l] * f[l];

    istate->fnorm2 = istate->err;
    p->params->errtoladj = powl((long double)10.0,
                                p->params->errtoldb / 10.0) * p->iterstate->fnorm2;

    if (istate->fnorm2 == 0.0)
        return LTFAT_DGTREALMP_STATUS_EMPTY;

    for (ltfat_int k = 0; k < p->P; k++)
    {
        /* LTFAT_REAL* sEl = istate->s[k]; */
        LTFAT_COMPLEX* cEl = istate->c[k];

        CHECKSTATUS(
            LTFAT_NAME(dgtreal_execute_ana_newarray)(p->dgtplans[k], f, cEl));

        for (ltfat_int n = 0; n < p->N[k]; n++)
        {
            LTFAT_NAME(maxtree_reset_complex)(istate->fmaxtree[k][n], cEl + n * p->M2[k]);
            LTFAT_NAME(maxtree_findmax)(istate->fmaxtree[k][n],
                                        &istate->maxcols[k][n],
                                        &istate->maxcolspos[k][n]);
        }

        LTFAT_NAME(maxtree_reset)(istate->tmaxtree[k], istate->maxcols[k]);

        memset( p->iterstate->suppind[k], 0 ,
                p->M2[k] * p->N[k] * sizeof * p->iterstate->suppind[k] );
    }

error:
    return status;
}


LTFAT_API int
LTFAT_NAME(dgtrealmp_execute_niters)(
    LTFAT_NAME(dgtrealmp_state)* p, ltfat_int itno, LTFAT_COMPLEX** cout)
{
    int status = LTFAT_DGTREALMP_STATUS_CANCONTINUE;

    LTFAT_NAME(dgtrealmpiter_state)* s = p->iterstate;

    if (s->fnorm2 == 0.0)
        return LTFAT_DGTREALMP_STATUS_EMPTY;

    for (ltfat_int iter = 0;
         iter < itno && status == LTFAT_DGTREALMP_STATUS_CANCONTINUE;
         iter++)
    {
        kpoint origpos;

        s->currit++;

        if ( LTFAT_NAME(dgtrealmp_execute_findmaxatom)(p, &origpos)
             != LTFATERR_SUCCESS )
            return LTFAT_DGTREALMP_STATUS_EMPTY;

        if ( !s->suppind[PTOI(origpos)] ) s->curratoms++;

        switch ( p->params->alg)
        {
        case ltfat_dgtmp_alg_MP:
            s->err -= LTFAT_NAME(dgtrealmp_execute_mp)( p, s->c[PTOI(origpos)], origpos,
                      cout);
            break;
        case ltfat_dgtmp_alg_LocOMP:
            status  = LTFAT_NAME(dgtrealmp_execute_locomp)( p, origpos, cout);
            break;
        case ltfat_dgtmp_alg_LocCyclicMP:
            status  = LTFAT_NAME(dgtrealmp_execute_cyclicmp)( p, origpos, cout);
            break;
        }

        if (s->err < 0)
            return LTFAT_DGTREALMP_STATUS_STALLED;

        if (s->err <= p->params->errtoladj)
            return LTFAT_DGTREALMP_STATUS_TOLREACHED;

        if (s->curratoms >= p->params->maxatoms)
            return LTFAT_DGTREALMP_STATUS_MAXATOMS;

        if (s->currit >= p->params->maxit)
            return LTFAT_DGTREALMP_STATUS_MAXITER;
    }

    return status;
}

LTFAT_API int
LTFAT_NAME(dgtrealmp_revert)(
    LTFAT_NAME(dgtrealmp_state)* p, LTFAT_COMPLEX** cout)
{
    LTFAT_NAME(dgtrealmpiter_state)* s = p->iterstate;

    for (ltfat_int w = 0; w < p->P; w++)
        for (ltfat_int n = 0; n < p->N[w]; n++)
            for (ltfat_int m = 0; m < p->M2[w]; m++)
            {
                kpoint pos = kpoint_init(m, n, w);
                if ( ltfat_norm(cout[PTOI(pos)]) > 0)
                {
                    s->err += LTFAT_NAME(dgtrealmp_execute_invmp)( p, pos, cout);
                }
            }
    return 0;
}


LTFAT_API int
LTFAT_NAME(dgtrealmp_execute)(
    LTFAT_NAME(dgtrealmp_state)* p,
    const LTFAT_REAL f[], LTFAT_COMPLEX* cout[], LTFAT_REAL fout[])
{
    int status = LTFATERR_FAILED, status2 = LTFATERR_FAILED;
    CHECKSTATUS( status2 = LTFAT_NAME(dgtrealmp_execute_decompose)(p,f,cout));
    CHECKSTATUS(
            LTFAT_NAME(dgtrealmp_execute_synthesize)(
                p, (const LTFAT_COMPLEX**)cout, p->chanmask, fout));
    return status2;
error:
    return status;
}

LTFAT_API int
LTFAT_NAME(dgtrealmp_execute_synthesize)(
    LTFAT_NAME(dgtrealmp_state)* p, const LTFAT_COMPLEX* c[], int dict_mask[], LTFAT_REAL f[])
{
    int status = LTFATERR_FAILED;
    CHECKNULL(p); CHECKNULL(c);  CHECKNULL(f);

    memset(f, 0, p->L * sizeof * f);

    for (ltfat_int k = 0; k < p->P; k++)
    {
        if(dict_mask == NULL || dict_mask[k])
        {
            CHECKNULL(c[k]);
            CHECKSTATUS(
                LTFAT_NAME(dgtreal_execute_syn_newarray)( p->dgtplans[k], c[k], f));
        }
    }

    return LTFATERR_SUCCESS;
error:
    return status;
}


LTFAT_API int
LTFAT_NAME(dgtrealmp_execute_decompose)(
    LTFAT_NAME(dgtrealmp_state)* p, const LTFAT_REAL f[], LTFAT_COMPLEX* c[])
{
    int status = LTFATERR_SUCCESS;
    int status2 = LTFATERR_SUCCESS;

    CHECKNULL(p); CHECKNULL(f); CHECKNULL(c);

    CHECKSTATUS( LTFAT_NAME(dgtrealmp_reset)( p, f));

    for (ltfat_int k = 0; k < p->P; k++)
    {
        CHECKNULL(c[k]);
        memset(c[k], 0, p->M2[k] * p->N[k] * sizeof * c[k]);
    }

    while ( LTFAT_DGTREALMP_STATUS_CANCONTINUE ==
            ( status2 = LTFAT_NAME(dgtrealmp_execute_niters)(
                            p, p->params->iterstep, c)))
    {
        if (p->params->verbose)
        {
            printf("Error %.2Lf, iteration %zu \n",
                   (long double) 10.0 * log10l( p->iterstate->err / p->iterstate->fnorm2 ),
                   p->iterstate->currit);
        }
    }

    CHECKSTATUS(status2);
    /* memset(fout, 0, p->L * sizeof * fout); */
    /*  */
    /* for (ltfat_int k = 0; k < p->P; k++) */
    /*     CHECKSTATUS( */
    /*         LTFAT_NAME(dgtreal_execute_syn_newarray)( p->dgtplans[k], cout[k], fout)); */

    return status2;
error:
    return status;
}

/* LTFAT_API int */
/* LTFAT_NAME(dgtrealmp_execute_synthesizeresidual)( */
/*     LTFAT_NAME(dgtrealmp_state)* p, int dict_mask[], LTFAT_REAL f[]) */
/* { */
/*     int status = LTFATERR_FAILED; */
/*     CHECKNULL(p); CHECKNULL(f); */
/*  */
/*     memset(f, 0, p->L * sizeof * f); */
/*  */
/*     for (ltfat_int k = 0; k < p->P; k++) */
/*     { */
/*         if(dict_mask == NULL || dict_mask[k]) */
/*         { */
/*             CHECKNULL(p->iterstate->c[k]); */
/*             CHECKSTATUS( */
/*                 LTFAT_NAME(dgtreal_execute_syn_newarray)(  */
/*                     p->dgtplans[k], p->iterstate->c[k], f)); */
/*         } */
/*     } */
/*  */
/*     return LTFATERR_SUCCESS; */
/* error: */
/*     return status; */
/* } */


LTFAT_API int
LTFAT_NAME(dgtrealmp_done)( LTFAT_NAME(dgtrealmp_state)** p)
{
    LTFAT_NAME(dgtrealmp_state)* pp;
    int status = LTFATERR_SUCCESS;
    CHECKNULL(p); CHECKNULL(*p);
    pp = *p;

    LTFAT_SAFEFREEALL(pp->a,pp->M,pp->M2,pp->N,pp->chanmask,pp->couttmp);


    if (pp->params)
        ltfat_dgtmp_params_free(pp->params);

    if (pp->closures)
    {
        for (ltfat_int k = 0; k < pp->P; k++)
               ltfat_free(pp->closures[k]);

        ltfat_free(pp->closures);
        pp->closures = NULL;
    }

    if (pp->dgtplans)
    {
        for (ltfat_int k = 0; k < pp->P; k++)
            LTFAT_NAME(dgtreal_done)(&pp->dgtplans[k]);

        ltfat_free(pp->dgtplans);
        pp->dgtplans = NULL;
    }

    if (pp->gramkerns)
    {
        for (ltfat_int k = 0; k < pp->P * pp->P; k++)
        {
                LTFAT_NAME(dgtrealmp_kernel_done)( &pp->gramkerns[k]);
        }

        ltfat_free(pp->gramkerns);
        pp->gramkerns = NULL;
    }

    if (pp->iterstate)
        LTFAT_NAME(dgtrealmpiter_done)(&pp->iterstate);

    ltfat_free(pp);
    *p = NULL;
error:
    return status;
}

int
LTFAT_NAME(dgtrealmpiter_init)(
    ltfat_int a[], ltfat_int M[], ltfat_int P, ltfat_int L,
    LTFAT_NAME(dgtrealmpiter_state)** state)
{
    LTFAT_NAME(dgtrealmpiter_state)* s = NULL;
    int status = LTFATERR_FAILED;

    CHECKNULL( state );
    CHECKMEM( s =    LTFAT_NEW( LTFAT_NAME(dgtrealmpiter_state)) );
    /* CHECKMEM( s->s = LTFAT_NEWARRAY(LTFAT_REAL*,    P)); */
    CHECKMEM( s->c = LTFAT_NEWARRAY(LTFAT_COMPLEX*, P));
    CHECKMEM( s->N = LTFAT_NEWARRAY(ltfat_int, P));
    CHECKMEM( s->suppind = LTFAT_NEWARRAY(unsigned int*, P));
    CHECKMEM( s->maxcols    =  LTFAT_NEWARRAY(LTFAT_REAL*, P));
    CHECKMEM( s->maxcolspos =  LTFAT_NEWARRAY(ltfat_int*, P));
    CHECKMEM( s->tmaxtree =  LTFAT_NEWARRAY( LTFAT_NAME(maxtree)*, P));
    CHECKMEM( s->fmaxtree =  LTFAT_NEWARRAY( LTFAT_NAME(maxtree)**, P));
    /* s->fmaxtree = NULL; */
    s->P = P;

    for (ltfat_int p = 0; p < P; p++)
    {
        ltfat_int N = L / a[p];
        s->N[p] = N;
        ltfat_int M2 = M[p] / 2 + 1;
        /* ltfat_int M2ext = M2 */
        /* CHECKMEM( s->s[p] = LTFAT_NAME_REAL(malloc)(N * M2) ); */
        CHECKMEM( s->c[p] = LTFAT_NAME_COMPLEX(malloc)(N * M2) );
        CHECKMEM( s->suppind[p] = LTFAT_NEWARRAY(unsigned int, N * M2 ));
        CHECKMEM( s->maxcols[p]    = LTFAT_NAME_REAL(malloc)(N) );
        CHECKMEM( s->maxcolspos[p] = LTFAT_NEWARRAY(ltfat_int, N) );
        CHECKSTATUS( LTFAT_NAME(maxtree_init)(N, N,
                                              ltfat_imax(0, ltfat_pow2base(ltfat_nextpow2(N)) - 4), &s->tmaxtree[p]));

        CHECKMEM( s->fmaxtree[p] = LTFAT_NEWARRAY(LTFAT_NAME(maxtree)*, N));
        for (ltfat_int n = 0; n < N; n++ )
        {
            CHECKSTATUS( LTFAT_NAME(maxtree_init)(
                             M2, M[p], ltfat_imax(0, ltfat_pow2base(ltfat_nextpow2(M[p])) - 4),
                             &s->fmaxtree[p][n]));
        }

    }

    *state = s;
    return LTFATERR_SUCCESS;
error:
    if (s) LTFAT_NAME(dgtrealmpiter_done)(&s);
    *state = NULL;
    return status;
}

int
LTFAT_NAME(dgtrealmpiter_done)(LTFAT_NAME(dgtrealmpiter_state)** state)
{
    LTFAT_NAME(dgtrealmpiter_state)* s = NULL;
    int status = LTFATERR_SUCCESS;
    CHECKNULL(state); CHECKNULL(*state);
    s = *state;

    if (s->s)
    {
        for (ltfat_int p = 0; p < s->P; p++)
            ltfat_safefree(s->s[p]);

        ltfat_free(s->s);
    }

    if (s->c)
    {
        for (ltfat_int p = 0; p < s->P; p++)
            ltfat_safefree(s->c[p]);

        ltfat_free(s->c);
    }

    if (s->suppind)
    {
        for (ltfat_int p = 0; p < s->P; p++)
            ltfat_safefree(s->suppind[p]);

        ltfat_free(s->suppind);
    }

    if (s->maxcols)
    {
        for (ltfat_int p = 0; p < s->P; p++)
            ltfat_safefree(s->maxcols[p]);

        ltfat_free(s->maxcols);
    }

    if (s->maxcolspos)
    {
        for (ltfat_int p = 0; p < s->P; p++)
            ltfat_safefree(s->maxcolspos[p]);

        ltfat_free(s->maxcolspos);
    }

    if (s->tmaxtree)
    {
        for (ltfat_int p = 0; p < s->P; p++)
            LTFAT_NAME(maxtree_done)(&s->tmaxtree[p]);

        ltfat_free(s->tmaxtree);
    }

    if (s->fmaxtree)
    {
        for (ltfat_int p = 0; p < s->P; p++)
        {
            for (ltfat_int n = 0; n < s->N[p]; n++)
                LTFAT_NAME(maxtree_done)(&s->fmaxtree[p][n]);

            ltfat_free(s->fmaxtree[p]);
        }

        ltfat_free(s->fmaxtree);
    }

    if (s->cvalModBuf)
    {
        for (ltfat_int p = 0; p < s->P * s->P; p++)
            ltfat_safefree(s->cvalModBuf[p]);

        ltfat_free(s->cvalModBuf);
    }

    ltfat_safefree(s->gramBuf);
    ltfat_safefree(s->cvalBuf);
    ltfat_safefree(s->cvalinvBuf);
    ltfat_safefree(s->cvalBufPos);
    ltfat_safefree(s->pBuf);
    if (s->hplan) LTFAT_NAME_COMPLEX(hermsystemsolver_done)(&s->hplan);
    ltfat_safefree(s->N);
    ltfat_free(s);
    *state = NULL;
error:
    return status;
}

LTFAT_API int
LTFAT_NAME(dgtrealmp_set_iterstep)(
    LTFAT_NAME(dgtrealmp_state)* p, size_t iterstep)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(p);
    CHECK(LTFATERR_NOTPOSARG, iterstep > 0, "iterstep must be greater than 0");
    p->params->iterstep = iterstep;
error:
    return status;
}

LTFAT_API int
LTFAT_NAME(dgtrealmp_set_maxatoms)(
    LTFAT_NAME(dgtrealmp_state)* p, size_t maxatoms)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(p);
    CHECK(LTFATERR_NOTPOSARG, maxatoms > 0, "maxatoms must be greater than 0");
    p->params->maxatoms = maxatoms;
    p->params->maxit = 2 * maxatoms;
error:
    return status;
}

LTFAT_API int
LTFAT_NAME(dgtrealmp_set_errtoldb)(
    LTFAT_NAME(dgtrealmp_state)* p, double errtoldb)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(p);
    CHECK(LTFATERR_BADARG, errtoldb <= 0, "errtoldb must be lower than 0");
    p->params->errtoldb = errtoldb;

error:
    return status;
}

LTFAT_API int
LTFAT_NAME(dgtrealmp_get_errdb)(
    const LTFAT_NAME(dgtrealmp_state)* p, double* err)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(p); CHECKNULL(err);

    *err = (double) (10.0 * log10l(p->iterstate->err / p->iterstate->fnorm2));
error:
    return status;
}

LTFAT_API int
LTFAT_NAME(dgtrealmp_get_numatoms)(
    const LTFAT_NAME(dgtrealmp_state)* p, size_t* atoms)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(p); CHECKNULL(atoms);

    *atoms = p->iterstate->curratoms;
error:
    return status;

}

LTFAT_API int
LTFAT_NAME(dgtrealmp_get_numiters)(
    const LTFAT_NAME(dgtrealmp_state)* p, size_t* iters)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(p); CHECKNULL(iters);

    *iters = p->iterstate->currit;
error:
    return status;
}


LTFAT_API LTFAT_NAME(dgtreal_plan)**
LTFAT_NAME(dgtrealmp_getdgtrealplan)(LTFAT_NAME(dgtrealmp_state)* p)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(p);

    return p->dgtplans;
error:
    return NULL;

}

LTFAT_API int
LTFAT_NAME(dgtrealmp_getresidualcoef_compact)(
    LTFAT_NAME(dgtrealmp_state)* p, LTFAT_COMPLEX* c)
{

    int status = LTFATERR_SUCCESS;
    ltfat_int accum = 0;
    CHECKNULL(p); CHECKNULL(c);

    for (ltfat_int k = 0; k < p->P; k++)
    {
        ltfat_int L = p->N[k] * p->M2[k];
        memcpy(c + accum, p->iterstate->c[k], L * sizeof * c);
        accum += L;
    }

error:
    return status;
}

LTFAT_API int
LTFAT_NAME(dgtrealmp_init_gen_compact)(
    const LTFAT_REAL g[], ltfat_int gl[], ltfat_int L, ltfat_int P, ltfat_int a[],
    ltfat_int M[], ltfat_dgtmp_params* params,
    LTFAT_NAME(dgtrealmp_state)** pout)
{
    int status = LTFATERR_SUCCESS;
    const LTFAT_REAL** multig = NULL;
    CHECK( LTFATERR_NOTPOSARG, P > 0, "P must be positive (passed %td)", P);
    CHECKMEM( multig = LTFAT_NEWARRAY(const LTFAT_REAL*, P));

    for (ltfat_int p = 0, glaccum = 0; p < P;  glaccum += gl[p], p++)
    {
        CHECK(LTFATERR_NOTPOSARG, gl[p] > 0,
              "gl[%td] must be positive (passed %td)", p, gl[p]);
        multig[p] = g + glaccum;
    }

    CHECKSTATUS(
        LTFAT_NAME(dgtrealmp_init_gen)(multig, gl, L, P, a, M, params, pout));

error:
    ltfat_safefree(multig);
    return status;
}

LTFAT_API int
LTFAT_NAME(dgtrealmp_execute_compact)(
    LTFAT_NAME(dgtrealmp_state)* p,
    const LTFAT_REAL* f, LTFAT_COMPLEX* cout, LTFAT_REAL* fout)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(p);
    for (ltfat_int k = 0, accum = 0; k < p->P; accum += p->N[k] * p->M2[k], k++)
        p->couttmp[k] = cout + accum;

    status = LTFAT_NAME(dgtrealmp_execute)( p, f, p->couttmp, fout);
error :
    return status;

}

LTFAT_API int
LTFAT_NAME(dgtrealmp_execute_niters_compact)(
    LTFAT_NAME(dgtrealmp_state)* p, ltfat_int itno, LTFAT_COMPLEX* cout)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(p);
    for (ltfat_int k = 0, accum = 0; k < p->P; accum += p->N[k] * p->M2[k], k++)
        p->couttmp[k] = cout + accum;

    status = LTFAT_NAME(dgtrealmp_execute_niters)( p, itno, p->couttmp);
error :
    return status;

}

/* LTFAT_API int */
/* LTFAT_NAME(dgtdict_coherence_gen)( */
/*     const LTFAT_REAL* g[], ltfat_int gl[], ltfat_int L, ltfat_int P, */
/*     ltfat_int a[], ltfat_int M[], LTFAT_REAL* coherence) */
/* { */
/*     int status = LTFATERR_SUCCESS; */
/* error: */
/*     return status; */
/* } */
/*  */
/* LTFAT_API int */
/* LTFAT_NAME(dgtdict_coherence_fromstatus)( */
/*     LTFAT_NAME(dgtrealmp_state)* p, LTFAT_REAL* coherence) */
/* { */
/*     int status = LTFATERR_SUCCESS; */
/*     LTFAT_REAL maxval = 0.0; */
/*     CHECKNULL(p); CHECKNULL(coherence); */
/*  */
/*     for (ltfat_int pp1 = 0; pp1 < p->P; pp1++) */
/*     { */
/*         LTFAT_NAME(kerns)* kk = p->gramkerns[pp1 + pp1 * p->P]; */
/*  */
/*         ltfat_int midlinpos = kk->mid.hmid + kk->mid.wmid * kk->size.height; */
/*  */
/*         for (ltfat_int n = 0; n < kk->size.width * kk->size.height; n++) */
/*             if (n != midlinpos && ltfat_abs(kk->kval[n]) > maxval) */
/*                 maxval = ltfat_abs(kk->kval[n]); */
/*  */
/*         for (ltfat_int pp2 = pp1 + 1; pp2 < p->P; pp2++) */
/*         { */
/*             LTFAT_NAME(kerns)* kk2 = p->gramkerns[pp2 + pp1 * p->P]; */
/*  */
/*             for (ltfat_int n = 0; n < kk2->size.width * kk2->size.height; n++) */
/*                 if (ltfat_abs(kk2->kval[n]) > maxval) */
/*                     maxval = ltfat_abs(kk2->kval[n]); */
/*         } */
/*     } */
/*  */
/*     *coherence = maxval; */
/* error: */
/*     return status; */
/* } */

/* LTFAT_API int */
/* LTFAT_NAME(dgtdict_coherence)( */
/*     LTFAT_NAME(dgtmp_parbuf)* pb, LTFAT_REAL* coherence) */
/* { */
/* } */
/*  */
/* LTFAT_API int */
/* LTFAT_NAME(dgtdict_babel)( */
/*     LTFAT_NAME(dgtmp_parbuf)* pb, ltfat_int P, LTFAT_REAL* babel) */
/* { */
/* } */
/*  */
/* LTFAT_API int */
/* LTFAT_NAME(dgtdict_isquaziincoherent)( */
/*     LTFAT_NAME(dgtmp_parbuf)* pb) */
/* { */
/* } */
