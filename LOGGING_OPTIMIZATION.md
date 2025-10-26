# Optimisation des Logs de la Stratégie

## Vue d'ensemble

Ce système permet de désactiver complètement les appels aux méthodes de logging au moment de la compilation, éliminant ainsi l'overhead des appels de fonction même lorsque les logs sont désactivés.

## Configuration

### Activer/Désactiver les logs

Dans le fichier `include/strategy.h`, vous trouverez la définition suivante :

```cpp
// Macro pour activer/désactiver les logs de la stratégie
// Décommenter la ligne suivante pour désactiver complètement les logs en production
// #define STRATEGY_DISABLE_LOGGING
```

**Pour désactiver les logs en production :**
```cpp
#define STRATEGY_DISABLE_LOGGING
```

**Pour activer les logs (mode par défaut) :**
```cpp
// #define STRATEGY_DISABLE_LOGGING
```

## Macros disponibles

### STRATEGY_LOG
Utilisée pour les appels de méthodes avec des paramètres :
```cpp
STRATEGY_LOG(logger, log_general, "Message de log", LogLevel::INFO);
STRATEGY_LOG(logger, log_signal, signal);
```

### STRATEGY_LOG_VOID
Utilisée pour les appels de méthodes sans paramètres :
```cpp
STRATEGY_LOG_VOID(logger, start_chrono);
STRATEGY_LOG_VOID(logger, clear);
```

## Comportement

### Mode avec logging (par défaut)
- Tous les appels aux méthodes de logging sont exécutés normalement
- Les logs sont générés selon la configuration de l'utilisateur
- Légère overhead due aux appels de fonction

### Mode sans logging (STRATEGY_DISABLE_LOGGING défini)
- **Tous les appels sont remplacés par `((void)0)` au moment de la compilation**
- Aucun code de logging n'est généré dans le binaire
- **Overhead nul** : aucun appel de fonction, aucune construction de chaîne
- Optimisation maximale pour la production

## Avantages

1. **Performance optimale en production** : Zero overhead quand les logs sont désactivés
2. **Code propre** : Pas besoin de `#ifdef` partout dans le code
3. **Flexibilité** : Un simple `#define` pour basculer entre les modes
4. **Sécurité** : Les erreurs de typage sont détectées à la compilation même en mode sans logging

## Utilisation typique

### Développement et Debug
Laisser le logging activé (défaut) :
```cpp
// #define STRATEGY_DISABLE_LOGGING
```

### Production et Release
Activer la désactivation complète :
```cpp
#define STRATEGY_DISABLE_LOGGING
```

### Benchmark et Profiling
Comparer les performances avec et sans logging pour mesurer l'impact précis.

## Migration du code existant

Remplacer :
```cpp
logger->log_general("Message");
logger->start_chrono();
```

Par :
```cpp
STRATEGY_LOG(logger, log_general, "Message");
STRATEGY_LOG_VOID(logger, start_chrono);
```

## Notes techniques

- Les macros utilisent `((void)0)` qui est complètement éliminé par le compilateur
- Les arguments des macros ne sont même pas évalués en mode désactivé
- Cela permet d'éviter la construction de chaînes coûteuses (comme les `fast_double_to_string`)
- Le code reste type-safe grâce au mécanisme de macros variadic
