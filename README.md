# GALILEO EFMS

## Overview
GALILEO EFMS is a C++ based system that handles files management with a focus on retention and archival capabilities.

## Features
- Archival policy implementation
- Retention policy implementation
- Logging service integration
- Database service management
- DDS policy configuration
- Vecow policy configuration

## Installation Requirements

### System Requirements
- C++ compiler
- Make build system

### Database Requirements
- PostgreSQL version 16.6 or above
- Libpqxx version 6.4.5 or above

### Installing Dependencies

#### PostgreSQL
```bash
# For Ubuntu/Debian
sudo apt-get update
sudo apt-get install postgresql-16
```

#### Libpqxx
```bash
# For Ubuntu/Debian
sudo apt-get install libpqxx-dev
```

### Installation
```bash
git clone <repository-url>
cd galileo-efms/scripts
```

## Running the Application

### Main Application
To run the main application:
```bash
./main_run.sh
```

### Tests
To run the test suite:
```bash
./tests_run.sh
```

## Development
The project follows a modular architecture with separate controllers and services for different functionalities:
- Archival Controller
- Retention Controller
- File Service
- Database Service
- Logging Service

## Support
For support and queries, please create an issue in the project repository.

---
*Note: This is a working document and will be updated as the project evolves.*
