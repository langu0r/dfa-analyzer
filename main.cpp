#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <map>

// Объявляем структуру ДО класса, чтобы она была видна
struct AnalysisResult {
    bool success;
    std::string message;
    int line;
    int position;
    std::string duplicateName;
};

class DFAAnalyzer {
private:
    enum State {
        Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q_ERROR
    };
    
    State currentState;
    std::set<std::string> declaredVariables;
    std::set<char> validTypeChars;
    std::set<char> validIdChars;
    std::set<char> operators;
    
    int currentLine;
    int currentPos;
    std::string currentIdentifier;
    std::string duplicateVarName;
    std::string currentType;
    
public:
    DFAAnalyzer() {
        // Инициализация допустимых символов
        for (char c = 'a'; c <= 'z'; c++) validTypeChars.insert(c);
        for (char c = 'A'; c <= 'Z'; c++) validTypeChars.insert(c);
        for (char c = '0'; c <= '9'; c++) validTypeChars.insert(c);
        validTypeChars.insert('_');
        
        validIdChars = validTypeChars;
        
        operators = {'=', '+', '-', '*', '/', '(', ')', '[', ']', '{', '}', '.', ','};
        
        reset();
    }
    
    void reset() {
        currentState = Q0;
        currentLine = 1;
        currentPos = 1;
        declaredVariables.clear();
        currentIdentifier.clear();
        duplicateVarName.clear();
        currentType.clear();
    }
    
    bool isWhitespace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }
    
    AnalysisResult analyze(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return {false, "Cannot open input file", 0, 0, ""};
        }
        
        std::string line;
        currentLine = 1;
        bool hasContent = false;
        
        while (std::getline(file, line)) {
            currentPos = 0;
            hasContent = false;
            
            for (char c : line) {
                if (!isWhitespace(c)) hasContent = true;
                processChar(c);
                if (currentState == Q_ERROR) {
                    file.close();
                    if (!duplicateVarName.empty()) {
                        return {false, "Duplicate variable name", currentLine, currentPos, duplicateVarName};
                    }
                    return {false, "Syntax error", currentLine, currentPos, ""};
                }
                currentPos++;
            }
            
            // После обработки строки проверяем состояние
            if (hasContent) {
                // Если в строке был контент и мы не в конечном состоянии - ошибка
                if (currentState != Q6 && currentState != Q0 && currentState != Q_ERROR) {
                    file.close();
                    return {false, "Missing semicolon at end of line", currentLine, currentPos, ""};
                }
            }
            
            // Обработка перевода строки
            processChar('\n');
            currentLine++;
        }
        
        file.close();
        
        // Проверяем конечное состояние после обработки всего файла
        if (currentState == Q6 || currentState == Q0) {
            return {true, "Correct variable declaration", 0, 0, ""};
        } else {
            return {false, "Unexpected end of input", currentLine, currentPos, ""};
        }
    }
    
private:
    void processChar(char c) {
        switch (currentState) {
            case Q0: // Начальное состояние, ожидание типа
                if (isWhitespace(c)) break;
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
                    currentState = Q1;
                    currentType += c;
                } else {
                    currentState = Q_ERROR;
                }
                break;
                
            case Q1: // Чтение типа данных
                if (isWhitespace(c)) {
                    currentState = Q2;
                } else if (c == ';' || c == '=') {
                    currentState = Q_ERROR; // Тип не может заканчиваться сразу на ; или =
                } else if (validTypeChars.count(c)) {
                    currentType += c;
                } else {
                    currentState = Q_ERROR;
                }
                break;
                
            case Q2: // Ожидание идентификатора после типа
                if (isWhitespace(c)) break;
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
                    currentState = Q3;
                    currentIdentifier += c;
                } else {
                    currentState = Q_ERROR;
                }
                break;
                
            case Q3: // Чтение идентификатора переменной
                if (isWhitespace(c)) {
                    currentState = Q4;
                    // Проверка на дублирование при завершении идентификатора
                    checkForDuplicate();
                } else if (c == ';') {
                    // Завершение объявления без инициализации
                    checkForDuplicate();
                    if (currentState != Q_ERROR) {
                        declaredVariables.insert(currentIdentifier);
                        currentState = Q6;
                        currentIdentifier.clear();
                        currentType.clear();
                    }
                } else if (c == '=') {
                    // Начало инициализации
                    checkForDuplicate();
                    if (currentState != Q_ERROR) {
                        declaredVariables.insert(currentIdentifier);
                        currentState = Q5;
                        currentIdentifier.clear();
                    }
                } else if (validIdChars.count(c)) {
                    currentIdentifier += c;
                } else {
                    currentState = Q_ERROR;
                }
                break;
                
            case Q4: // Ожидание ; или = после идентификатора
                if (isWhitespace(c)) break;
                if (c == ';') {
                    currentState = Q6;
                    currentIdentifier.clear();
                    currentType.clear();
                } else if (c == '=') {
                    currentState = Q5;
                    currentIdentifier.clear();
                } else {
                    currentState = Q_ERROR;
                }
                break;
                
            case Q5: // Чтение выражения после =
                if (c == ';') {
                    currentState = Q6;
                    currentType.clear();
                }
                // В выражении допускаем различные символы
                else if (c == '\n') {
                    // Перевод строки в середине выражения - ошибка
                    currentState = Q_ERROR;
                }
                break;
                
            case Q6: // Успешное завершение объявления
                if (isWhitespace(c)) {
                    if (c == '\n') {
                        currentState = Q0; // Начинаем новое объявление
                    }
                } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
                    currentState = Q1; // Новое объявление
                    currentType += c;
                } else {
                    currentState = Q_ERROR;
                }
                break;
                
            case Q_ERROR:
                // Уже в состоянии ошибки
                break;
        }
    }
    
    void checkForDuplicate() {
        if (declaredVariables.count(currentIdentifier)) {
            duplicateVarName = currentIdentifier;
            currentState = Q_ERROR;
        }
    }
};

int main() {
    DFAAnalyzer analyzer;
    AnalysisResult result = analyzer.analyze("input.txt");
    
    std::ofstream output("output.txt");
    if (result.success) {
        output << "Correct variable declaration" << std::endl;
        std::cout << "Correct variable declaration" << std::endl;
    } else {
        output << "Error: " << result.message << std::endl;
        std::cout << "Error: " << result.message << std::endl;
        if (result.line > 0) {
            output << "At line " << result.line << ", position " << result.position + 1 << std::endl;
            std::cout << "At line " << result.line << ", position " << result.position + 1 << std::endl;
        }
        if (!result.duplicateName.empty()) {
            output << "Duplicate variable: " << result.duplicateName << std::endl;
            std::cout << "Duplicate variable: " << result.duplicateName << std::endl;
        }
    }
    output.close();

    std::cout << std::endl << "Press Enter to exit...";
    std::cin.get();
    
    return 0;
}