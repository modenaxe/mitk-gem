#ifndef __GraphCut_h__
#define __GraphCut_h__

#include "ImageGraphCut3DKolmogorovFilter.hxx"

namespace GraphCut
{
    template<typename TInput, typename TForeground, typename TBackground, typename TOutput>
    using FilterType = itk::ImageGraphCut3DKolmogorovFilter<TInput, TForeground, TBackground, TOutput>;
}

#endif //__GraphCut_h__
