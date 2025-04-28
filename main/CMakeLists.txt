idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES "certs/AmazonRootCA1.pem" "certs/Node1certificate.crt" "certs/Node1private.key")
