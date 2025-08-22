#version 450 core

in vec3 fragNormal; // Incoming normal from vertex shader
out vec4 FragColor;

void main()
{
    // Normalize the fragment normal
    vec3 normal = normalize(fragNormal);

    // Simple shading based on the normal vector
    vec3 lightDir = normalize(vec3(-0.5, 0.5, 0.707)); // Light direction pointing along Z axis
    float intensity = max(dot(normal, lightDir), 0.0); // Lambertian reflectance

    // Base color
    vec3 baseColor = vec3(1.0, 0.5, 0.2); // Orange

    // Apply shading based on the normal's alignment with lightDir
    vec3 shadedColor = baseColor * intensity;

    // Set the output fragment color
    FragColor = vec4(shadedColor, 1.0);
}
