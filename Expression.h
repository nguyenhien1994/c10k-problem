#pragma once

class InvalidExprException : public std::runtime_error {
public:
    explicit InvalidExprException() : std::runtime_error("InvalidExprException") {}
};

class Expression {
private:
    std::string mExpression;
    int mCur;
    int64_t mResult;

    char peek() {
        return mExpression[mCur];
    }

    char getCur() {
        return mExpression[mCur++];
    }

    int64_t number() {
        int64_t result = getCur() - '0';
        while (peek() >= '0' && peek() <= '9') {
            result = 10 * result + getCur() - '0';
        }
        return result;
    }

    int64_t getFactor() {
        if (peek() >= '0' && peek() <= '9') {
            return number();
        } else if (peek() == '(') {
            getCur(); /* '(' */
            int64_t result = calExpression();
            getCur(); /* ')' */
            return result;
        } else if (peek() == '-') {
            getCur();
            return -getFactor();
        } else {
            throw InvalidExprException();
        }
    }

    int64_t getTerm() {
        int64_t result = getFactor();
        while (peek() == '*' || peek() == '/')
            if (getCur() == '*')
                result *= getFactor();
            else
                result /= getFactor();
        return result;
    }

    int64_t calExpression() {
        int64_t result = getTerm();
        while (peek() == '+' || peek() == '-')
            if (getCur() == '+')
                result += getTerm();
            else
                result -= getTerm();
        return result;
    }

public:
    Expression(const std::string &expression);
    ~Expression();

    int64_t getResult() const { return this->mResult; }
};

Expression::Expression(const std::string &expression) : mExpression(expression), mCur(0) {
    mResult = this->calExpression();
}

Expression::~Expression() {}
