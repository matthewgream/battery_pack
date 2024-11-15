
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <array>
#include <cstdlib>

namespace gaussian {

template <typename T>
using vector4 = std::array<T, 4>;
template <typename T>
using matrix4 = vector4<vector4<T>>;

String solve (matrix4<double> &matrix, vector4<double> &result) {
    static constexpr double DETERMINANT_DEMINIMUS = 1e-10;

    const double determinant = matrix [0][0] * (matrix [1][1] * matrix [2][2] * matrix [3][3] + matrix [1][2] * matrix [2][3] * matrix [3][1] + matrix [1][3] * matrix [2][1] * matrix [3][2] - matrix [1][3] * matrix [2][2] * matrix [3][1] - matrix [1][2] * matrix [2][1] * matrix [3][3] - matrix [1][1] * matrix [2][3] * matrix [3][2]);
    if (std::abs (determinant) < DETERMINANT_DEMINIMUS)
        return "matrix is singular/near-singular, determinant: " + ArithmeticToString (determinant, 12);

    for (size_t i = 0; i < 4; i++) {
        for (size_t j = i + 1; j < 4; j++) {
            const double factor = matrix [j][i] / matrix [i][i];
            for (size_t k = i; k < 4; k++)
                matrix [j][k] -= factor * matrix [i][k];
            result [j] -= factor * result [i];
        }
    }
    for (int i = 4 - 1; i >= 0; i--) {
        for (int j = i + 1; j < 4; j++)
            result [i] -= matrix [i][j] * result [j];
        result [i] /= matrix [i][i];
    }
    return String ();
}

String solve (matrix4<double> &XtX, vector4<double> &XtY, vector4<double> &result) {
    static constexpr double CONDITION_DEMAXIMUS = 1e15, DETERMINANT_DEMINIMUS = 1e-10;

    double max_singular = std::numeric_limits<double>::min (), min_singular = std::numeric_limits<double>::max ();
    for (int i = 0; i < 4; i++) {
        double sum_singular = 0;
        for (int j = 0; j < 4; j++)
            sum_singular += std::abs (XtX [i][j]);
        if (sum_singular > max_singular)
            max_singular = sum_singular;
        if (sum_singular < min_singular)
            min_singular = sum_singular;
    }
    const double condition_number = max_singular / min_singular;
    if (condition_number > CONDITION_DEMAXIMUS)
        return "matrix ill-conditioned, condition number estimate: " + ArithmeticToString (condition_number, 12);
    const double determinant =
        XtX [0][0] * (XtX [1][1] * XtX [2][2] * XtX [3][3] + XtX [1][2] * XtX [2][3] * XtX [3][1] + XtX [1][3] * XtX [2][1] * XtX [3][2] - XtX [1][3] * XtX [2][2] * XtX [3][1] - XtX [1][2] * XtX [2][1] * XtX [3][3] - XtX [1][1] * XtX [2][3] * XtX [3][2]) - XtX [0][1] * (XtX [1][0] * XtX [2][2] * XtX [3][3] + XtX [1][2] * XtX [2][3] * XtX [3][0] + XtX [1][3] * XtX [2][0] * XtX [3][2] - XtX [1][3] * XtX [2][2] * XtX [3][0] - XtX [1][2] * XtX [2][0] * XtX [3][3] - XtX [1][0] * XtX [2][3] * XtX [3][2]) + XtX [0][2] * (XtX [1][0] * XtX [2][1] * XtX [3][3] + XtX [1][1] * XtX [2][3] * XtX [3][0] + XtX [1][3] * XtX [2][0] * XtX [3][1] - XtX [1][3] * XtX [2][1] * XtX [3][0] - XtX [1][1] * XtX [2][0] * XtX [3][3] - XtX [1][0] * XtX [2][3] * XtX [3][1]) - XtX [0][3] * (XtX [1][0] * XtX [2][1] * XtX [3][2] + XtX [1][1] * XtX [2][2] * XtX [3][0] + XtX [1][2] * XtX [2][0] * XtX [3][1] - XtX [1][2] * XtX [2][1] * XtX [3][0] - XtX [1][1] * XtX [2][0] * XtX [3][2] - XtX [1][0] * XtX [2][2] * XtX [3][1]);
    if (std::abs (determinant) < DETERMINANT_DEMINIMUS)
        return "matrix is singular/near-singular, determinant: " + ArithmeticToString (determinant, 12);

    for (int i = 0; i < 4; i++) {
        int max_row = i;
        for (int j = i + 1; j < 4; j++)
            if (std::abs (XtX [j][i]) > std::abs (XtX [max_row][i]))
                max_row = j;
        if (max_row != i)
            std::swap (XtX [i], XtX [max_row]), std::swap (XtY [i], XtY [max_row]);
        for (int j = i + 1; j < 4; j++) {
            const double factor = XtX [j][i] / XtX [i][i];
            for (int k = i; k < 4; k++)
                XtX [j][k] -= factor * XtX [i][k];
            XtY [j] -= factor * XtY [i];
        }
    }
    for (int i = 3; i >= 0; i--) {
        result [i] = XtY [i];
        for (int j = i + 1; j < 4; j++)
            result [i] -= XtX [i][j] * result [j];
        result [i] /= XtX [i][i];
    }
    return String ();
}
}    // namespace gaussian

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
