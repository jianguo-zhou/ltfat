#include "ltfat.h"
#include "ltfat/types.h"
#include "ltfat/macros.h"
#include "dgtrealwrapper_private.h"

int
ltfat_dgtreal_params_defaults(ltfat_dgtreal_params* params)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(params);
    params->ptype = LTFAT_FREQINV;
    params->fftw_flags = FFTW_ESTIMATE;
    params->hint = ltfat_dgtreal_auto;
error:
    return status;
}

LTFAT_API ltfat_dgtreal_params*
ltfat_dgtreal_params_allocdef()
{
    ltfat_dgtreal_params* params;
    int status = LTFATERR_SUCCESS;
    CHECKMEM( params = LTFAT_NEW(ltfat_dgtreal_params));

    ltfat_dgtreal_params_defaults(params);
error:
    return params;
}

LTFAT_API int
ltfat_dgtreal_params_set_phaseconv(ltfat_dgtreal_params* params,
                                      ltfat_phaseconvention ptype)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(params);
    params->ptype = ptype;
error:
    return status;
}

LTFAT_API int
ltfat_dgtreal_params_set_fftwflags(ltfat_dgtreal_params* params,
                                      unsigned fftw_flags)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(params);
    params->fftw_flags = fftw_flags;
error:
    return status;

}

LTFAT_API int
ltfat_dgtreal_params_set_hint(ltfat_dgtreal_params* params,
                                 ltfat_dgtreal_hint hint)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(params);
    params->hint = hint;
error:
    return status;
}

LTFAT_API int
ltfat_dgtreal_params_free(ltfat_dgtreal_params* params)
{
    int status = LTFATERR_SUCCESS;
    CHECKNULL(params);
    ltfat_free(params);
error:
    return status;
}
